#pragma once

#include <Arduino.h>

void initIrSensor();
void updateIrSensor();
bool isIrDetected();
uint8_t getIrPin();
