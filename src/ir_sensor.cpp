#include "ir_sensor.h"

#include <Preferences.h>
#include "dfplayer.h"

namespace
{
constexpr uint8_t IR_PIN = 2; // GPIO2; LOW = beam broken (object detected)
constexpr const char *NVS_NAMESPACE = "ir_sensor";
constexpr const char *NVS_KEY_COUNT = "animalCount";
constexpr const char *NVS_KEY_BOOTS = "bootCount";

bool irDetected = false;
bool irPrevDetected = false;
uint32_t animalDetectedCount = 0;
uint32_t bootCount = 0;
Preferences preferences;
}

void initIrSensor()
{
    pinMode(IR_PIN, INPUT_PULLUP);
    preferences.begin(NVS_NAMESPACE, false);
    animalDetectedCount = preferences.getUInt(NVS_KEY_COUNT, 0);
    bootCount = preferences.getUInt(NVS_KEY_BOOTS, 0) + 1;
    preferences.putUInt(NVS_KEY_BOOTS, bootCount);
    Serial.print("[IR] Loaded animal count from NVS: ");
    Serial.println(animalDetectedCount);
    Serial.print("[IR] Boot count: ");
    Serial.println(bootCount);
}

void updateIrSensor()
{
    irDetected = digitalRead(IR_PIN) == HIGH;

    if (irDetected && !irPrevDetected)
    {
        ++animalDetectedCount;
        preferences.putUInt(NVS_KEY_COUNT, animalDetectedCount);
        Serial.print("[IR] pos edge: ");
        Serial.println(getIrDetectedString());
        if (!isSoundSequenceRunning() && !isSequenceUserStopped())
        {
            Serial.print("[IR] Start Sound Sequence");
            startSoundSequence(SequenceMode::ONCE);
        }
    }
    else if (!irDetected && irPrevDetected)
    {
        Serial.print("[IR] neg edge:  ");
        Serial.println(getIrDetectedString());
    }

    irPrevDetected = irDetected;
}

bool isIrDetected()
{
    return irDetected;
}

const char *getIrDetectedString()
{
    return irDetected ? "animal detected" : "no animal";
}

uint8_t getIrPin()
{
    return IR_PIN;
}

uint32_t getAnimalDetectedCount()
{
    return animalDetectedCount;
}

uint32_t getBootCount()
{
    return bootCount;
}
