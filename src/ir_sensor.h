#pragma once

#include <Arduino.h>

void initIrSensor();
void updateIrSensor();
bool isIrDetected();
const char *getIrDetectedString();
uint8_t getIrPin();
uint32_t getAnimalDetectedCount();
uint32_t getBootCount();
