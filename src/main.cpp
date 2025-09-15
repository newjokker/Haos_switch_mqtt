#include <Arduino.h>
#include "WiFi.h"
#include "MqttHelper.h"
#include "WiFiHelper.h"

const char* WIFI_SSID = "Saturn-Guest-2.4g";
const char* WIFI_PASSWORD = "Tuxingkeji-0918";
const char* MQTT_SERVER = "8.153.160.138";
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "mqtt_relay_test";
const int RELAY_PIN = 2;

MqttHelper mqttHelper(MQTT_SERVER, MQTT_PORT, MQTT_CLIENT_ID);
WiFiHelper wifiHelper(WIFI_SSID, WIFI_PASSWORD);

void publishRelayState() {
    const char* state = digitalRead(RELAY_PIN) ? "OPEN" : "CLOSED";
    char message[20];
    snprintf(message, sizeof(message), "relay:%s", state);
    mqttHelper.publish("device/status", message);
    Serial.printf("状态已更新: %s\n", state);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    Serial.printf("📩 主题=%s, 内容=%s\n", topic, msg.c_str());

    if (String(topic) == "control/relay") {
        digitalWrite(RELAY_PIN, msg.equalsIgnoreCase("OPEN") ? HIGH : LOW);
        publishRelayState();
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    wifiHelper.connect();
    mqttHelper.begin();
    mqttHelper.setCallback(mqttCallback);
}

void loop() {
    static unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_INTERVAL = 10000;

    if (!mqttHelper.isConnected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("尝试连接MQTT...");
            if (mqttHelper.connect()) {
                mqttHelper.publish("device/status", "online");
                mqttHelper.subscribe("control/relay");
                publishRelayState();
                Serial.println("✅ MQTT连接成功");
            } else {
                Serial.println("❌ 连接失败");
            }
            lastReconnectAttempt = now;
        }
    }
    mqttHelper.loop();
}