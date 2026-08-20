#pragma once

#include <Arduino.h>

using SoundTriggerCallback = bool (*)(const String &soundName);

void setupWebServer(bool *filesystemMountedState, bool *wifiConnectedState, String *selectedSoundState, SoundTriggerCallback soundTriggerCallback);
void handleWebServerClient();
String getAssemblyJson();