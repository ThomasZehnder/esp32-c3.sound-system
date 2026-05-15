#include "ir_sensor.h"

namespace
{
constexpr uint8_t IR_PIN = 0; // GPIO0; LOW = beam broken (object detected)

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
        Serial.println("[IR] pos edge: beam broken");
    }
    else if (!irDetected && irPrevDetected)
    {
        Serial.println("[IR] neg edge: beam clear");
    }

    irPrevDetected = irDetected;
}

bool isIrDetected()
{
    return irDetected;
}

uint8_t getIrPin()
{
    return IR_PIN;
}
