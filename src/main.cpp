#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "credentials.h"
#include "dfplayer.h"
#include "display.h"
#include "app_webserver.h"
#include "ir_sensor.h"
#include "mqtt_client.h"
#include "ultrasound_pwm.h"

// ====== PIN CONFIG ======
#define LED_PIN 8 // blue led on ESP32-C3-DevKitM-1, GPIO8, is connected to GND via a resistor, so HIGH turns it ON

namespace
{
constexpr uint8_t DFPLAYER_INIT_ATTEMPTS = 4;
constexpr uint32_t DFPLAYER_INITIAL_DELAY_MS = 750;
constexpr uint32_t DFPLAYER_RETRY_DELAY_MS = 500;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

struct WifiCredential
{
    const char *ssid;
    const char *password;
};

constexpr WifiCredential WIFI_CREDENTIALS[] = {
    {WIFI_SSID_1, WIFI_PASSWORD_1},
    {WIFI_SSID_2, WIFI_PASSWORD_2},
    {WIFI_SSID_3, WIFI_PASSWORD_3},
};

void logBootStep(const String &message)
{
    Serial.print("[BOOT] ");
    Serial.println(message);
}

bool connectToConfiguredWifi()
{
    for (const WifiCredential &credential : WIFI_CREDENTIALS)
    {
        logBootStep("connect WiFi " + String(credential.ssid));
        renderDisplay("ESP32-C3 OLED", "Connecting", credential.ssid, "");

        Serial.print("Connecting to WiFi: ");
        Serial.println(credential.ssid);

        WiFi.disconnect(true, true);
        delay(100);
        WiFi.begin(credential.ssid, credential.password);

        const unsigned long wifiStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS)
        {
            delay(250);
            Serial.print('.');
        }

        Serial.println();
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print("WiFi connected. IP: ");
            Serial.println(WiFi.localIP());
            return true;
        }

        Serial.print("WiFi connection failed for ");
        Serial.println(credential.ssid);
    }

    return false;
}
}

// ====== VARIABLES ======
unsigned long lastBlink = 0;
bool ledState = false;
bool wifiConnected = false;
bool filesystemMounted = false;
String selectedSound = "none";
bool lastSequenceRunning = false;

void setup()
{
    Serial.begin(115200);
    unsigned long serialStart = millis();
    while (!Serial && millis() - serialStart < 3000)
    {
        delay(10);
    }
    delay(200);

    logBootStep("setup start");

    pinMode(LED_PIN, OUTPUT);
    initIrSensor();

    // Init display
    logBootStep("init display");
    initDisplay();
    renderDisplay("ESP32-C3 OLED", "Init OK", "", "");

    logBootStep("prepare DFPlayer init");
    delay(DFPLAYER_INITIAL_DELAY_MS);

    for (uint8_t attempt = 1; attempt <= DFPLAYER_INIT_ATTEMPTS && !isDfPlayerReady(); ++attempt)
    {
        logBootStep("DFPlayer init attempt " + String(attempt) + "/" + String(DFPLAYER_INIT_ATTEMPTS));
        if (attempt > 1)
        {
            renderDisplay("ESP32-C3 OLED", "DFP retry", "Attempt " + String(attempt), "Recovering...");
            delay(DFPLAYER_RETRY_DELAY_MS);
        }

        initDfPlayer();
    }

    logBootStep(isDfPlayerReady() ? "DFPlayer ready" : "DFPlayer unavailable");

    logBootStep("init ultrasound");
    initUltrasoundPwm();

    logBootStep("start WiFi");
    WiFi.mode(WIFI_STA);
    wifiConnected = connectToConfiguredWifi();
    if (wifiConnected)
    {
        logBootStep("WiFi connected");
        logBootStep("init NTP");
        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
        logBootStep("init MQTT");
        setupMqttClient();
    }
    else
    {
        Serial.println("WiFi connection failed.");
    }

    logBootStep("mount LittleFS");
    filesystemMounted = LittleFS.begin(true);
    if (filesystemMounted)
    {
        Serial.println("LittleFS mounted.");
        if (!loadSoundSequenceFromFilesystem())
        {
            Serial.println("Using compiled default sound sequence.");
        }
        setupWebServer(&filesystemMounted, &wifiConnected, &selectedSound, playSoundByName);
    }
    else
    {
        Serial.println("LittleFS mount failed.");
    }

    logBootStep("setup done");
    Serial.println("Setup done.");
}

void loop()
{
    wifiConnected = WiFi.status() == WL_CONNECTED;
    updateIrSensor();
    handleWebServerClient();
    updateDfPlayerScheduler();
    updateUltrasoundPwm();
    updateMqttClient();

    const bool sequenceRunning = isSoundSequenceRunning();
    if (sequenceRunning && !lastSequenceRunning)
    {
        publishAssemblyNow();
    }
    lastSequenceRunning = sequenceRunning;

    // ===== LED BLINK =====
    if (millis() - lastBlink > 500)
    {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        //Serial.println(String("LED state changed: ") + (ledState ? "ON" : "OFF"));
    }

    // ===== DISPLAY UPDATE =====
    char ledText[16];
    char millisText[24];
    char networkText[24];
    String ipAddress = WiFi.localIP().toString();
    String audioStatusText;

    snprintf(ledText, sizeof(ledText), "LED: %s", ledState ? "ON" : "OFF");
    snprintf(millisText, sizeof(millisText), "%lus", millis() / 1000);
    snprintf(networkText, sizeof(networkText), "%s", wifiConnected ? ipAddress.c_str() : "No WiFi");

    if (isUltrasoundPlaying())
    {
        audioStatusText = isUltrasoundRandomMode()
                              ? "US " + String(getUltrasoundFrequencyHz()) + "Hz rnd"
                              : "US " + String(getUltrasoundFrequencyHz()) + "Hz";
    }
    else
    {
        audioStatusText = isDfPlayerReady() ? selectedSound : "DFP offline";
    }

    renderDisplay("ESP32-C3 running", networkText, audioStatusText, millisText);

    delay(100);
}