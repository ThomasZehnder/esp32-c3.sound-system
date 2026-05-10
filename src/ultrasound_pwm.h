#pragma once

#include <Arduino.h>

void initUltrasoundPwm();
void updateUltrasoundPwm();
bool playUltrasound(uint32_t frequencyHz, uint8_t volumePercent, uint32_t playTimeMs);
bool playRandomUltrasound(uint32_t minFrequencyHz, uint32_t maxFrequencyHz, uint8_t volumePercent, uint32_t playTimeMs);
void stopUltrasound();
bool isUltrasoundPlaying();
bool isUltrasoundRandomMode();
uint32_t getUltrasoundFrequencyHz();
uint8_t getUltrasoundVolumePercent();
uint32_t getUltrasoundRemainingMs();
uint8_t getUltrasoundPin();
uint32_t getUltrasoundMinFrequencyHz();
uint32_t getUltrasoundMaxFrequencyHz();
uint32_t getUltrasoundRandomMinFrequencyHz();
uint32_t getUltrasoundRandomMaxFrequencyHz();