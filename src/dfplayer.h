#pragma once

#include <Arduino.h>

#include "soundconfig.h"

void initDfPlayer();
bool isDfPlayerReady();
bool playSoundByName(const String &soundName);
const SoundDefinition *getSoundDefinitions(size_t &count);
const UltrasoundSequenceDefinition *getUltrasoundSequenceDefinitions(size_t &count);
const SoundSequenceStep *getSoundSequence(size_t &count);
size_t getMaxSoundSequenceSteps();
bool setSoundSequence(const SoundSequenceStep *steps, size_t count);
bool loadSoundSequenceFromFilesystem();
bool saveSoundSequenceToFilesystem();
bool isSoundSequenceLoadedFromFilesystem();
uint8_t getDfPlayerVolume();
uint8_t getDfPlayerMinVolume();
uint8_t getDfPlayerMaxVolume();
uint8_t getGlobalVolumePercent();
bool setGlobalVolumePercent(uint8_t volumePercent);
uint8_t getNormalizedUltrasoundVolumePercent();
SoundSequenceSettings getSoundSequenceSettings();
bool setSoundSequenceSettings(const SoundSequenceSettings &settings);
bool setDfPlayerVolume(uint8_t volume);
bool startSoundSequence();
void stopSoundSequence();
bool isSoundSequenceRunning();
void updateDfPlayerScheduler();
int getSoundSequenceCurrentIndex();
bool isSoundSequenceStepActive();
unsigned long getSoundSequenceElapsedMs();
unsigned long getSoundSequenceRemainingMs();
