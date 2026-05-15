#include "ir_sensor.h"

#include "dfplayer.h"

namespace
{
constexpr uint8_t IR_PIN = 2; // GPIO2; LOW = beam broken (object detected)

bool irDetected = false;
bool irPrevDetected = false;
}

void initIrSensor()
{
    pinMode(IR_PIN, INPUT_PULLUP);
}

void updateIrSensor()
{
    irDetected = digitalRead(IR_PIN) == HIGH;

    if (irDetected && !irPrevDetected)
    {
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
