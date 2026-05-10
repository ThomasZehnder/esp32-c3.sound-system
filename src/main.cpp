#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "credentials.h"
#include "dfplayer.h"
#include "display.h"
#include "app_webserver.h"
#include "ultrasound_pwm.h"

// ====== PIN CONFIG ======
#define LED_PIN 8 // blue led on ESP32-C3-DevKitM-1, GPIO8, is connected to GND via a resistor, so HIGH turns it ON

namespace
{
constexpr uint8_t DFPLAYER_INIT_ATTEMPTS = 4;
constexpr uint32_t DFPLAYER_INITIAL_DELAY_MS = 750;
constexpr uint32_t DFPLAYER_RETRY_DELAY_MS = 500;

void logBootStep(const String &message)
{
    Serial.print("[BOOT] ");
    Serial.println(message);
}
}

// ====== VARIABLES ======
unsigned long lastBlink = 0;
bool ledState = false;
bool wifiConnected = false;
bool filesystemMounted = false;
String selectedSound = "none";

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
    WiFi.begin(WIFI_SSID_1, WIFI_PASSWORD_1);

    renderDisplay("ESP32-C3 OLED", "Connecting", WIFI_SSID_1, "");

    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID_1);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000)
    {
        delay(250);
        Serial.print('.');
    }

    wifiConnected = WiFi.status() == WL_CONNECTED;
    Serial.println();
    if (wifiConnected)
    {
        Serial.print("WiFi connected. IP: ");
        Serial.println(WiFi.localIP());
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
    handleWebServerClient();
    updateDfPlayerScheduler();
    updateUltrasoundPwm();

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