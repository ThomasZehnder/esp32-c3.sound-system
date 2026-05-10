#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "credentials.h"
#include "dfplayer.h"
#include "display.h"
#include "app_webserver.h"

// ====== PIN CONFIG ======
#define LED_PIN 8 // blue led on ESP32-C3-DevKitM-1, GPIO8, is connected to GND via a resistor, so HIGH turns it ON

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

    pinMode(LED_PIN, OUTPUT);

    // Init display
    initDisplay();
    renderDisplay("ESP32-C3 OLED", "Init OK", "", "");

    initDfPlayer();

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

    Serial.println("Setup done.");
}

void loop()
{
    wifiConnected = WiFi.status() == WL_CONNECTED;
    handleWebServerClient();
    updateDfPlayerScheduler();

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

    snprintf(ledText, sizeof(ledText), "LED: %s", ledState ? "ON" : "OFF");
    snprintf(millisText, sizeof(millisText), "%lus", millis() / 1000);
    snprintf(networkText, sizeof(networkText), "%s", wifiConnected ? ipAddress.c_str() : "No WiFi");

    renderDisplay("ESP32-C3 running", networkText, isDfPlayerReady() ? selectedSound : "DFP offline", millisText);

    delay(100);
}