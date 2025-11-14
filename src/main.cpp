#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>  // 需要安装 ArduinoJson 库

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
const char* mqtt_client_id = "lvdi_desktop_relay";
const char* state_topic = "lvdi/jokker_desktop/relay/state";
const char* command_topic = "lvdi/jokker_desktop/relay/command";
const char* availability_topic = "lvdi/jokker_desktop/relay/availability";
const char* ha_config_topic = "homeassistant/switch/lvdi_jokker_desktop_relay/config";

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
  return "relay_" + mac;
}

// =========================
// MQTT HA 自动发现配置生成
// =========================
String generateHADiscoveryConfig() {
  String uid = getUniqueID();
  
  // 使用 ArduinoJson 库生成正确的 JSON
  DynamicJsonDocument doc(1024);
  
  // 基本配置
  doc["name"] = deviceDescription;
  doc["unique_id"] = uid;
  doc["state_topic"] = state_topic;
  doc["command_topic"] = command_topic;
  doc["availability_topic"] = availability_topic;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  doc["state_on"] = "ON";
  doc["state_off"] = "OFF";
  doc["optimistic"] = false;
  doc["retain"] = true;
  
  // 设备信息
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"][0] = uid;
  device["name"] = locationName;
  device["manufacturer"] = "CustomMQTTDevice";
  device["model"] = "MQTTRelayV1";
  device["sw_version"] = "1.0";
  
  String configPayload;
  serializeJson(doc, configPayload);
  
  Serial.println("Generated HA Discovery Config:");
  Serial.println(configPayload);
  
  return configPayload;
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
  Serial.print("AP IP: ");
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
    "<h2>WiFi 配置</h2>"
    "<form method='POST' action='/save'>"
    "<input name='ssid' placeholder='WiFi 名称 (SSID)' required>"
    "<input name='pass' type='password' placeholder='WiFi 密码' required>"
    "<input name='location' placeholder='设备位置' value='" + locationName + "'>"
    "<input name='description' placeholder='设备描述' value='" + deviceDescription + "'>"
    "<button type='submit'>保存并重启</button>"
    "</form></div></body></html>";

  server.on("/", HTTP_GET, [htmlPage]() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String location = server.arg("location");
    String description = server.arg("description");
    
    if (ssid.length() > 0 && pass.length() > 0) {
      saveWifiConfig(ssid, pass);
      saveDeviceConfig(location, description);
      server.send(200, "text/html", 
        "<html><body><h2>配置保存成功!</h2><p>设备即将重启...</p></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/html", 
        "<html><body><h2>错误!</h2><p>SSID 和密码不能为空</p></body></html>");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
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
  
  Serial.printf("MQTT 收到消息 [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == command_topic) {
    if (msg == "ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
      Serial.println("继电器: ON");
    } else if (msg == "OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
      Serial.println("继电器: OFF");
    }
    
    // 发布状态更新
    mqttClient.publish(state_topic, relayState ? "ON" : "OFF", true);
  }
}

// =========================
// MQTT 重连和 HA 自动发现
// =========================
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("尝试连接 MQTT...");
    
    if (mqttClient.connect(mqtt_client_id, availability_topic, 0, true, "offline")) {
      Serial.println("MQTT 连接成功!");
      
      // 订阅命令主题
      mqttClient.subscribe(command_topic);
      Serial.println("已订阅命令主题: " + String(command_topic));
      
      // 发布在线状态
      mqttClient.publish(availability_topic, "online", true);
      
      // 发布 Home Assistant 自动发现配置
      String configPayload = generateHADiscoveryConfig();
      if (mqttClient.publish(ha_config_topic, configPayload.c_str(), true)) {
        Serial.println("HA 自动发现配置发布成功!");
      } else {
        Serial.println("HA 自动发现配置发布失败!");
      }
      
      // 发布当前状态
      mqttClient.publish(state_topic, relayState ? "ON" : "OFF", true);
      
    } else {
      Serial.print("MQTT 连接失败, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" 5秒后重试...");
      delay(5000);
    }
  }
}

// =========================
// WiFi 连接
// =========================
void connectWiFi() {
  Serial.printf("连接 WiFi: %s\n", ssidSaved.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidSaved.c_str(), passSaved.c_str());

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > 30000) { // 30秒超时
      Serial.println("WiFi 连接超时，进入 AP 模式");
      clearWifiConfig();
      ESP.restart();
      return;
    }
    
    if (checkBootLongPress()) {
      Serial.println("检测到 BOOT 长按，进入 AP 模式");
      clearWifiConfig();
      ESP.restart();
      return;
    }
    
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println();
  Serial.print("WiFi 连接成功! IP: ");
  Serial.println(WiFi.localIP());
}

// =========================
// 主流程
// =========================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 MQTT 继电器启动 ===");
  
  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // 加载配置
  loadWifiConfig();
  loadDeviceConfig();
  
  Serial.println("设备信息:");
  Serial.println("  MAC: " + WiFi.macAddress());
  Serial.println("  位置: " + locationName);
  Serial.println("  描述: " + deviceDescription);

  // 检查是否进入配网模式
  if (checkBootLongPress() || ssidSaved == "" || passSaved == "") {
    Serial.println("进入 AP 配网模式");
    shouldEnterAP = true;
    startAPMode();
  } else {
    connectWiFi();
    
    // 设置 MQTT
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048); // 增加缓冲区大小
    
    reconnectMQTT();
  }
}

void loop() {
  if (shouldEnterAP) {
    server.handleClient();
    return;
  }

  // 处理 MQTT
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // 定期上报在线状态
  if (millis() - lastAvailabilityReport > AVAILABILITY_INTERVAL) {
    mqttClient.publish(availability_topic, "online", true);
    lastAvailabilityReport = millis();
    Serial.println("上报在线状态");
  }

  // 检查 BOOT 长按
  if (checkBootLongPress()) {
    Serial.println("检测到 BOOT 长按，清除配置并重启");
    clearWifiConfig();
    delay(1000);
    ESP.restart();
  }
}