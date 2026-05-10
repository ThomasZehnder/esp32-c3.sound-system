#pragma once

#include <Arduino.h>

void initUltrasoundPwm();
void updateUltrasoundPwm();
bool playUltrasound(uint32_t frequencyHz, uint8_t volumePercent, uint32_t playTimeMs);
void stopUltrasound();