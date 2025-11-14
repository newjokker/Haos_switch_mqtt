#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>

Preferences prefs;
WebServer server(80);

#define BOOT_PIN 0
#define BOOT_HOLD_MS 3000
#define RELAY_PIN 2   // 继电器控制引脚

String ssidSaved = "";
String passSaved = "";
bool shouldEnterAP = false;

// 设备信息
String locationName = "UnknownRoom";
String deviceDescription = "MQTT Relay Device";

// MQTT 配置
const char* mqtt_server = "8.153.160.138";
const char* mqtt_client_id = "txkj_desktop_relay";
const char* state_topic = "txkj/jokker_desktop/relay/state";
const char* command_topic = "txkj/jokker_desktop/relay/command";
const char* availability_topic = "txkj/jokker_desktop/relay/availability";
const char* ha_config_topic = "homeassistant/switch/txkj_jokker_desktop_relay/config";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool relayState = false;
unsigned long lastAvailabilityReport = 0;
const unsigned long AVAILABILITY_INTERVAL = 300000; // 5分钟

// =========================
// 工具函数
// =========================
void saveWifiConfig(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void loadWifiConfig() {
  prefs.begin("wifi", true);
  ssidSaved = prefs.getString("ssid", "");
  passSaved = prefs.getString("pass", "");
  prefs.end();
}

void clearWifiConfig() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

void saveDeviceConfig(const String &location, const String &description) {
  prefs.begin("device", false);
  prefs.putString("location", location);
  prefs.putString("description", description);
  prefs.end();
}

void loadDeviceConfig() {
  prefs.begin("device", true);
  locationName = prefs.getString("location", "UnknownRoom");
  deviceDescription = prefs.getString("description", "MQTT Relay Device");
  prefs.end();
}

String getUniqueID() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return mac;
}

// =========================
// BOOT 长按检测
// =========================
bool checkBootLongPress() {
  if (digitalRead(BOOT_PIN) == LOW) {
    unsigned long start = millis();
    while (digitalRead(BOOT_PIN) == LOW) {
      if (millis() - start >= BOOT_HOLD_MS) {
        return true;
      }
      delay(20);
    }
  }
  return false;
}

// =========================
// AP 配网模式
// =========================
void startAPMode() {
  Serial.println("Starting AP mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup", "12345678");
  Serial.println(WiFi.softAPIP());

  String htmlPage =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body{font-family:Arial;background:#f2f2f2;text-align:center;padding-top:60px;}"
    ".card{background:white;margin:0 auto;padding:25px;border-radius:10px;max-width:320px;"
    "box-shadow:0 0 10px rgba(0,0,0,0.15);}"
    "input{width:100%;padding:10px;margin-top:10px;border-radius:5px;border:1px solid #ccc;}"
    "button{margin-top:15px;padding:10px 20px;width:100%;background:#007BFF;color:white;"
    "border:none;border-radius:5px;font-size:16px;}"
    "</style></head><body>"
    "<div class='card'>"
    "<h2>WiFi CONFIG </h2>"
    "<form method='POST' action='/save'>"
    "<input name='ssid' placeholder='WiFi Name (SSID)' required>"
    "<input name='pass' type='password' placeholder='WiFi Password' required>"
    "<input name='location' placeholder='Location'>"
    "<input name='description' placeholder='Describe'>"
    "<button type='submit'>Save And Restart</button>"
    "</form></div></body></html>";

  server.on("/", HTTP_GET, [htmlPage]() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String location = server.arg("location");
    String description = server.arg("description");
    saveWifiConfig(ssid, pass);
    saveDeviceConfig(location, description);
    server.send(200, "text/html", "Save Success, Machine Will Restart Soon");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

// =========================
// MQTT 回调
// =========================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();
  Serial.printf("MQTT Received [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == command_topic) {
    if (msg == "ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
    } else if (msg == "OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
    }
    mqttClient.publish(state_topic, relayState ? "ON" : "OFF", true);
  }
}

// =========================
// MQTT 初始化 & HA 自动发现
// =========================
void setupMQTT() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  loadDeviceConfig();
  String uid = getUniqueID();

  while (!mqttClient.connected()) {
    Serial.println("Connecting MQTT...");
    if (mqttClient.connect(mqtt_client_id, NULL, NULL, availability_topic, 0, true, "offline")) {
      Serial.println("MQTT connected");
      
      // 等待数据稳定
      delay(500);
      
      mqttClient.subscribe(command_topic);

      // 上报在线
      mqttClient.publish(availability_topic, "online", true);

      // 发布 Home Assistant 自动发现配置
      String configPayload = "{"
        "\"name\": \"" + deviceDescription + "\","
        "\"unique_id\": \"" + uid + "\","
        "\"state_topic\": \"" + String(state_topic) + "\","
        "\"command_topic\": \"" + String(command_topic) + "\","
        "\"device\": {"
            "\"identifiers\": [\"" + uid + "\"],"
            "\"name\": \"" + locationName + "\","
            "\"manufacturer\": \"CustomMQTTDevice\","
            "\"model\": \"MQTTRelayV1\""
        "},"
        "\"payload_on\": \"ON\","
        "\"payload_off\": \"OFF\","
        "\"state_on\": \"ON\","
        "\"state_off\": \"OFF\","
        "\"optimistic\": false,"
        "\"availability_topic\": \"" + String(availability_topic) + "\","
        "\"payload_available\": \"online\","
        "\"payload_not_available\": \"offline\""
      "}";
      mqttClient.publish(ha_config_topic, configPayload.c_str(), true);
      mqttClient.publish(state_topic, relayState ? "ON" : "OFF", true);

      Serial.println(configPayload.c_str());

    } else {
      Serial.println("MQTT connect failed → retrying in 1s");
      delay(1000);
    }
  }
}

// =========================
// WiFi 连接
// =========================
void connectWiFiForever() {
  Serial.printf("Connecting to %s\n", ssidSaved.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidSaved.c_str(), passSaved.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    if (checkBootLongPress()) {
      Serial.println("BOOT long press during WiFi retry → Enter AP mode");
      clearWifiConfig();
      ESP.restart();
    }
    Serial.println("WiFi connect failed → retrying");
    delay(1000);
  }
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}

// =========================
// 主流程
// =========================
void setup() {
  Serial.begin(115200);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  loadWifiConfig();

  if (checkBootLongPress()) {
    Serial.println("BOOT long press detected (boot stage)");
    clearWifiConfig();
    shouldEnterAP = true;
  }

  if (ssidSaved == "" || passSaved == "") {
    Serial.println("WiFi config empty → AP mode");
    shouldEnterAP = true;
  }

  if (shouldEnterAP) {
    startAPMode();
  } else {
    connectWiFiForever();
    setupMQTT();
  }
}

void loop() {
  if (shouldEnterAP) server.handleClient();

  if (!shouldEnterAP) {
    mqttClient.loop();

    // 定期上报在线状态
    if (millis() - lastAvailabilityReport > AVAILABILITY_INTERVAL) {
      mqttClient.publish(availability_topic, "online", true);
      lastAvailabilityReport = millis();
    }

    // 支持 BOOT 长按进入 AP
    if (checkBootLongPress()) {
      Serial.println("BOOT long press → Enter AP mode");
      clearWifiConfig();
      ESP.restart();
    }
  }
}
