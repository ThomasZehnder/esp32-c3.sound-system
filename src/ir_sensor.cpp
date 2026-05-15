#include "ir_sensor.h"

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
    irDetected = digitalRead(IR_PIN) == LOW;

    if (irDetected && !irPrevDetected)
    {
        Serial.println("[IR] pos edge: no animal");
    }
    else if (!irDetected && irPrevDetected)
    {
        Serial.println("[IR] neg edge: animal detected");
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
