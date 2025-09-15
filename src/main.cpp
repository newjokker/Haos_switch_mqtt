#include <Arduino.h>
#include "WiFi.h"
#include "MqttHelper.h"
#include "WiFiHelper.h"

// ================= 配置信息 =================
const char* WIFI_SSID     = "Saturn-Guest-2.4g";
const char* WIFI_PASSWORD = "Tuxingkeji-0918";

const char* MQTT_SERVER   = "8.153.160.138";
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT_ID = "mqtt_relay_test";              

// 继电器控制引脚
const int RELAY_PIN = 2;  // 根据实际接线修改

// 工具类
MqttHelper mqttHelper(MQTT_SERVER, MQTT_PORT, MQTT_CLIENT_ID);
WiFiHelper wifiHelper(WIFI_SSID, WIFI_PASSWORD);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("📩 收到消息 | 主题 = ");
    Serial.print(topic);
    Serial.print(" | 内容 = ");

    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.println(msg);

    // 检查是否是控制继电器的主题
    if (String(topic) == "control/relay") {
        if (msg.equalsIgnoreCase("OPEN")) {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("继电器已打开");
        } else {
            digitalWrite(RELAY_PIN, LOW); 
            Serial.println("继电器已关闭");
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // 初始化继电器引脚
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // 初始状态为关闭

    wifiHelper.connect();
    mqttHelper.begin();
    mqttHelper.setCallback(mqttCallback);
}

void loop() {
    // 如果 MQTT 掉线，尝试重连
    if (!mqttHelper.isConnected()) {
        if (mqttHelper.connect()) {
            // 连接成功后发送上线消息
            mqttHelper.publish("device/status", "Relay controller online");
            // 订阅控制主题
            mqttHelper.subscribe("control/relay");
            Serial.println("MQTT已连接并订阅控制主题");
        }
    }

    mqttHelper.loop();   // 必须常驻执行
}