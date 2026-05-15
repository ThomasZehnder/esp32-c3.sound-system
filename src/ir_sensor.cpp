#include "ir_sensor.h"

#include <Preferences.h>
#include "dfplayer.h"

namespace
{
constexpr uint8_t IR_PIN = 2; // GPIO2; LOW = beam broken (object detected)
constexpr const char *NVS_NAMESPACE = "ir_sensor";
constexpr const char *NVS_KEY_COUNT = "animalCount";

bool irDetected = false;
bool irPrevDetected = false;
uint32_t animalDetectedCount = 0;
Preferences preferences;
}

void initIrSensor()
{
    pinMode(IR_PIN, INPUT_PULLUP);
    preferences.begin(NVS_NAMESPACE, false);
    animalDetectedCount = preferences.getUInt(NVS_KEY_COUNT, 0);
    Serial.print("[IR] Loaded animal count from NVS: ");
    Serial.println(animalDetectedCount);
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
