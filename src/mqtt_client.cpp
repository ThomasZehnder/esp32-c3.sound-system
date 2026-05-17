#include "mqtt_client.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "app_webserver.h"
#include "credentials.h"

namespace
{
constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 60000;
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

unsigned long lastPublishMs = 0;
unsigned long lastReconnectAttemptMs = 0;

String buildClientId()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return "esp32-soundsystem-" + mac;
}

String buildTopic()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return "soundsystem/" + mac + "/assembly";
}

void reconnect()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    const unsigned long now = millis();
    if (now - lastReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS)
        return;

    lastReconnectAttemptMs = now;
    Serial.print("[MQTT] Connecting to ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);

    if (mqttClient.connect(buildClientId().c_str()))
    {
        Serial.println("[MQTT] Connected.");
    }
    else
    {
        Serial.print("[MQTT] Connect failed, rc=");
        Serial.println(mqttClient.state());
    }
}
} // namespace

void setupMqttClient()
{
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(1024);
}

void publishAssemblyNow()
{
    if (!mqttClient.connected())
        return;

    const String topic = buildTopic();
    const String payload = getAssemblyJson();
    const bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), true);
    lastPublishMs = millis();

    Serial.print("[MQTT] Publish ");
    Serial.print(topic);
    Serial.println(ok ? " OK" : " FAILED");
}

void updateMqttClient()
{
    if (!mqttClient.connected())
    {
        reconnect();
        return;
    }

    mqttClient.loop();

    if (millis() - lastPublishMs >= MQTT_PUBLISH_INTERVAL_MS)
    {
        publishAssemblyNow();
    }
}
