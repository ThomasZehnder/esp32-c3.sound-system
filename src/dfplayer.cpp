#include "dfplayer.h"

#include <ArduinoJson.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <LittleFS.h>

#include "ultrasound_pwm.h"

namespace
{
constexpr int DFPLAYER_RX_PIN = 3;
constexpr int DFPLAYER_TX_PIN = 4;
constexpr uint8_t DFPLAYER_MIN_VOLUME = 0;
constexpr uint8_t DFPLAYER_MAX_VOLUME = 30;
constexpr uint8_t DFPLAYER_VOLUME = 10;
constexpr uint8_t DEFAULT_SEQUENCE_START_VOLUME_PERCENT = 33;
constexpr size_t SOUND_STEP_KIND_BUFFER_LENGTH = 16;
constexpr size_t SOUND_STEP_TAG_BUFFER_LENGTH = 24;
constexpr size_t SOUND_STEP_DESCRIPTION_BUFFER_LENGTH = 80;
constexpr char SOUND_SEQUENCE_FILE_PATH[] = "/sequence.json";

HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;
bool dfPlayerReady = false;
uint8_t currentVolume = DFPLAYER_VOLUME;
bool sequenceRunning = false;
size_t sequenceIndex = 0;
unsigned long sequenceStepStartedAt = 0;
bool sequenceStepActive = false;
SoundSequenceStep runtimeSequence[MAX_SOUND_SEQUENCE_STEPS];
char runtimeSequenceKinds[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_KIND_BUFFER_LENGTH];
char runtimeSequenceTags[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_TAG_BUFFER_LENGTH];
char runtimeSequenceDescriptions[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_DESCRIPTION_BUFFER_LENGTH];
size_t runtimeSequenceCount = 0;
bool sequenceLoadedFromFilesystem = false;
SoundSequenceSettings sequenceSettings = {DEFAULT_SEQUENCE_START_VOLUME_PERCENT};

uint8_t clampPercent(int value)
{
    if (value < 0)
    {
        return 0;
    }

    if (value > 100)
    {
        return 100;
    }

    return static_cast<uint8_t>(value);
}

uint8_t volumeToPercent(uint8_t volume)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(volume) * 100U + (DFPLAYER_MAX_VOLUME / 2U)) / DFPLAYER_MAX_VOLUME);
}

uint8_t percentToVolume(uint8_t volumePercent)
{
    const uint8_t clampedPercent = clampPercent(volumePercent);
    return static_cast<uint8_t>((static_cast<uint16_t>(clampedPercent) * DFPLAYER_MAX_VOLUME + 50U) / 100U);
}

const SoundDefinition *findSoundDefinition(const String &soundName)
{
    for (const SoundDefinition &definition : SOUND_DEFINITIONS)
    {
        if (soundName == definition.tag)
        {
            return &definition;
        }
    }

    return nullptr;
}

const SoundDefinition *findSoundDefinitionByTag(const char *soundTag)
{
    if (!soundTag)
    {
        return nullptr;
    }

    for (const SoundDefinition &definition : SOUND_DEFINITIONS)
    {
        if (strcmp(soundTag, definition.tag) == 0)
        {
            return &definition;
        }
    }

    return nullptr;
}

const UltrasoundSequenceDefinition *findUltrasoundDefinitionByTag(const char *soundTag)
{
    if (!soundTag)
    {
        return nullptr;
    }

    for (const UltrasoundSequenceDefinition &definition : ULTRASOUND_SEQUENCE_DEFINITIONS)
    {
        if (strcmp(soundTag, definition.tag) == 0)
        {
            return &definition;
        }
    }

    return nullptr;
}

void writeRuntimeSequenceStep(size_t index, const char *kind, const char *tag, uint32_t durationMs, const char *description)
{
    snprintf(runtimeSequenceKinds[index], sizeof(runtimeSequenceKinds[index]), "%s", kind ? kind : "pause");
    snprintf(runtimeSequenceTags[index], sizeof(runtimeSequenceTags[index]), "%s", tag ? tag : "");
    snprintf(runtimeSequenceDescriptions[index], sizeof(runtimeSequenceDescriptions[index]), "%s", description ? description : "");

    runtimeSequence[index].kind = runtimeSequenceKinds[index];
    runtimeSequence[index].tag = runtimeSequenceTags[index];
    runtimeSequence[index].durationMs = durationMs;
    runtimeSequence[index].description = runtimeSequenceDescriptions[index];
}

bool isValidSequenceKind(const char *kind)
{
    return kind && (strcmp(kind, "sound") == 0 || strcmp(kind, "pause") == 0 || strcmp(kind, "ultrasound") == 0);
}

String buildSequenceDescription(const char *kind, const char *tag)
{
    if (kind && strcmp(kind, "pause") == 0)
    {
        return "Pause step";
    }

    if (kind && strcmp(kind, "ultrasound") == 0)
    {
        const UltrasoundSequenceDefinition *definition = findUltrasoundDefinitionByTag(tag);
        if (!definition)
        {
            return "Configured ultrasound";
        }

        return String(definition->description);
    }

    const SoundDefinition *definition = findSoundDefinitionByTag(tag);
    if (!definition)
    {
        return "Configured sound";
    }

    return String(definition->description);
}

void loadDefaultSoundSequence()
{
    setSoundSequence(DEFAULT_SOUND_SEQUENCE, sizeof(DEFAULT_SOUND_SEQUENCE) / sizeof(DEFAULT_SOUND_SEQUENCE[0]));
    sequenceSettings.startVolumePercent = volumeToPercent(DFPLAYER_VOLUME);
    sequenceLoadedFromFilesystem = false;
}

bool applySoundSequenceJson(JsonArrayConst steps)
{
    const size_t stepCount = steps.size();
    if (stepCount == 0 || stepCount > MAX_SOUND_SEQUENCE_STEPS)
    {
        return false;
    }

    SoundSequenceStep tempSteps[MAX_SOUND_SEQUENCE_STEPS];
    char tempKinds[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_KIND_BUFFER_LENGTH] = {};
    char tempTags[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_TAG_BUFFER_LENGTH] = {};
    char tempDescriptions[MAX_SOUND_SEQUENCE_STEPS][SOUND_STEP_DESCRIPTION_BUFFER_LENGTH] = {};

    size_t index = 0;
    for (JsonObjectConst step : steps)
    {
        const char *kind = step["kind"] | "";
        const char *tag = step["tag"] | "";
        const uint32_t durationMs = step["durationMs"] | 0;

        snprintf(tempKinds[index], sizeof(tempKinds[index]), "%s", kind);
        snprintf(tempTags[index], sizeof(tempTags[index]), "%s", tag);
        snprintf(tempDescriptions[index], sizeof(tempDescriptions[index]), "%s", strcmp(kind, "pause") == 0 ? "Pause step" : tag);

        tempSteps[index].kind = tempKinds[index];
        tempSteps[index].tag = tempTags[index];
        tempSteps[index].durationMs = durationMs;
        tempSteps[index].description = tempDescriptions[index];
        ++index;
    }

    return setSoundSequence(tempSteps, stepCount);
}

bool readSoundSequenceFromFile(File &file)
{
    JsonDocument document;
    DeserializationError error = deserializeJson(document, file);
    if (error)
    {
        Serial.print("Sequence JSON parse failed: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonArrayConst steps = document["sequence"].as<JsonArrayConst>();
    if (steps.isNull())
    {
        Serial.println("Sequence JSON missing sequence array.");
        return false;
    }

    const int startVolumePercent = document["startVolumePercent"] | DEFAULT_SEQUENCE_START_VOLUME_PERCENT;

    if (!applySoundSequenceJson(steps))
    {
        return false;
    }

    sequenceSettings.startVolumePercent = clampPercent(startVolumePercent);
    return true;
}

bool playResolvedSound(const SoundDefinition &definition)
{
    dfPlayer.playFolder(definition.folder, definition.track);
    Serial.print("DFPlayer play folder ");
    Serial.print(definition.folder);
    Serial.print(" track ");
    Serial.print(definition.track);
    Serial.print(" for ");
    Serial.println(definition.tag);
    return true;
}

bool playResolvedUltrasound(const UltrasoundSequenceDefinition &definition, uint32_t durationMs)
{
    const uint8_t normalizedVolumePercent = getNormalizedUltrasoundVolumePercent();

    if (definition.randomMode)
    {
        return playRandomUltrasound(definition.minFrequencyHz, definition.maxFrequencyHz, normalizedVolumePercent, durationMs);
    }

    return playUltrasound(definition.minFrequencyHz, normalizedVolumePercent, durationMs);
}

bool sequenceRequiresDfPlayer()
{
    for (size_t index = 0; index < runtimeSequenceCount; ++index)
    {
        if (strcmp(runtimeSequence[index].kind, "sound") == 0)
        {
            return true;
        }
    }

    return false;
}
}

void initDfPlayer()
{
    loadDefaultSoundSequence();
    dfPlayerReady = false;
    dfPlayerSerial.end();
    delay(50);
    dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
    delay(150);

    if (!dfPlayer.begin(dfPlayerSerial, true, true))
    {
        Serial.println("DFPlayer init failed.");
        return;
    }

    dfPlayer.volume(currentVolume);
    dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
    dfPlayerReady = true;

    Serial.print("DFPlayer ready on RX ");
    Serial.print(DFPLAYER_RX_PIN);
    Serial.print(" TX ");
    Serial.println(DFPLAYER_TX_PIN);
}

bool isDfPlayerReady()
{
    return dfPlayerReady;
}

const SoundDefinition *getSoundDefinitions(size_t &count)
{
    count = sizeof(SOUND_DEFINITIONS) / sizeof(SOUND_DEFINITIONS[0]);
    return SOUND_DEFINITIONS;
}

const UltrasoundSequenceDefinition *getUltrasoundSequenceDefinitions(size_t &count)
{
    count = sizeof(ULTRASOUND_SEQUENCE_DEFINITIONS) / sizeof(ULTRASOUND_SEQUENCE_DEFINITIONS[0]);
    return ULTRASOUND_SEQUENCE_DEFINITIONS;
}

const SoundSequenceStep *getSoundSequence(size_t &count)
{
    count = runtimeSequenceCount;
    return runtimeSequence;
}

size_t getMaxSoundSequenceSteps()
{
    return MAX_SOUND_SEQUENCE_STEPS;
}

bool setSoundSequence(const SoundSequenceStep *steps, size_t count)
{
    if (!steps || count == 0 || count > MAX_SOUND_SEQUENCE_STEPS)
    {
        return false;
    }

    for (size_t index = 0; index < count; ++index)
    {
        const SoundSequenceStep &step = steps[index];
        if (!isValidSequenceKind(step.kind) || step.durationMs == 0)
        {
            return false;
        }

        if (strcmp(step.kind, "sound") == 0)
        {
            const SoundDefinition *definition = findSoundDefinitionByTag(step.tag);
            if (!definition || strcmp(definition->tag, "stop") == 0)
            {
                return false;
            }
        }

        if (strcmp(step.kind, "ultrasound") == 0)
        {
            const UltrasoundSequenceDefinition *definition = findUltrasoundDefinitionByTag(step.tag);
            if (!definition)
            {
                return false;
            }
        }
    }

    stopSoundSequence();

    for (size_t index = 0; index < count; ++index)
    {
        const SoundSequenceStep &step = steps[index];
        const String description = buildSequenceDescription(step.kind, step.tag);
        writeRuntimeSequenceStep(index, step.kind, strcmp(step.kind, "pause") == 0 ? "" : step.tag, step.durationMs, description.c_str());
    }

    runtimeSequenceCount = count;
    sequenceIndex = 0;
    sequenceStepStartedAt = 0;
    sequenceStepActive = false;
    return true;
}

bool loadSoundSequenceFromFilesystem()
{
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS not available for sequence load.");
        return false;
    }

    if (!LittleFS.exists(SOUND_SEQUENCE_FILE_PATH))
    {
        Serial.println("No sequence.json found on LittleFS, using default sequence.");
        return false;
    }

    File file = LittleFS.open(SOUND_SEQUENCE_FILE_PATH, "r");
    if (!file)
    {
        Serial.println("Failed to open sequence.json for reading.");
        return false;
    }

    const bool ok = readSoundSequenceFromFile(file);
    file.close();

    if (ok)
    {
        sequenceLoadedFromFilesystem = true;
        Serial.println("Loaded sequence.json from LittleFS.");
    }

    return ok;
}

bool saveSoundSequenceToFilesystem()
{
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS not available for sequence save.");
        return false;
    }

    File file = LittleFS.open(SOUND_SEQUENCE_FILE_PATH, "w");
    if (!file)
    {
        Serial.println("Failed to open sequence.json for writing.");
        return false;
    }

    JsonDocument document;
    document["maxSteps"] = getMaxSoundSequenceSteps();
    document["startVolumePercent"] = sequenceSettings.startVolumePercent;
    JsonArray sequenceArray = document["sequence"].to<JsonArray>();

    for (size_t index = 0; index < runtimeSequenceCount; ++index)
    {
        JsonObject step = sequenceArray.add<JsonObject>();
        step["kind"] = runtimeSequence[index].kind;
        step["tag"] = runtimeSequence[index].tag;
        step["durationMs"] = runtimeSequence[index].durationMs;
    }

    const size_t bytesWritten = serializeJson(document, file);
    file.close();

    if (bytesWritten == 0)
    {
        Serial.println("Failed to write sequence.json.");
        return false;
    }

    Serial.println("Persisted sequence.json to LittleFS.");
    sequenceLoadedFromFilesystem = true;
    return true;
}

bool isSoundSequenceLoadedFromFilesystem()
{
    return sequenceLoadedFromFilesystem;
}

uint8_t getDfPlayerVolume()
{
    return currentVolume;
}

uint8_t getDfPlayerMinVolume()
{
    return DFPLAYER_MIN_VOLUME;
}

uint8_t getDfPlayerMaxVolume()
{
    return DFPLAYER_MAX_VOLUME;
}

uint8_t getGlobalVolumePercent()
{
    return volumeToPercent(currentVolume);
}

bool setGlobalVolumePercent(uint8_t volumePercent)
{
    return setDfPlayerVolume(percentToVolume(volumePercent));
}

uint8_t getNormalizedUltrasoundVolumePercent()
{
    return static_cast<uint8_t>((static_cast<uint16_t>(getGlobalVolumePercent()) * 10U + 50U) / 100U);
}

SoundSequenceSettings getSoundSequenceSettings()
{
    return sequenceSettings;
}

bool setSoundSequenceSettings(const SoundSequenceSettings &settings)
{
    if (settings.startVolumePercent > 100)
    {
        return false;
    }

    sequenceSettings.startVolumePercent = settings.startVolumePercent;
    return true;
}

bool setDfPlayerVolume(uint8_t volume)
{
    if (volume < DFPLAYER_MIN_VOLUME || volume > DFPLAYER_MAX_VOLUME)
    {
        return false;
    }

    currentVolume = volume;

    if (!dfPlayerReady)
    {
        return false;
    }

    dfPlayer.volume(currentVolume);
    Serial.print("DFPlayer volume set to ");
    Serial.println(currentVolume);
    return true;
}

bool playSoundByName(const String &soundName)
{
    if (!dfPlayerReady)
    {
        Serial.println("DFPlayer not ready.");
        return false;
    }

    if (soundName == "stop")
    {
        stopSoundSequence();
        Serial.println("DFPlayer stop.");
        return true;
    }

    const SoundDefinition *definition = findSoundDefinition(soundName);
    if (!definition)
    {
        Serial.print("Unknown sound name: ");
        Serial.println(soundName);
        return false;
    }

    sequenceRunning = false;
    sequenceStepActive = false;
    return playResolvedSound(*definition);
}

bool startSoundSequence()
{
    setGlobalVolumePercent(sequenceSettings.startVolumePercent);

    if (!dfPlayerReady)
    {
        if (sequenceRequiresDfPlayer())
        {
            Serial.println("DFPlayer sequence start failed: player not ready.");
            return false;
        }
    }

    size_t stepCount = 0;
    getSoundSequence(stepCount);
    if (stepCount == 0)
    {
        Serial.println("DFPlayer sequence start failed: no steps configured.");
        return false;
    }

    sequenceRunning = true;
    sequenceIndex = 0;
    sequenceStepActive = false;
    sequenceStepStartedAt = 0;
    Serial.println("DFPlayer sequence started.");
    return true;
}

void stopSoundSequence()
{
    sequenceRunning = false;
    sequenceStepActive = false;
    stopUltrasound();
    if (dfPlayerReady)
    {
        dfPlayer.stop();
    }
}

bool isSoundSequenceRunning()
{
    return sequenceRunning;
}

int getSoundSequenceCurrentIndex()
{
    if (!sequenceRunning)
    {
        return -1;
    }

    return static_cast<int>(sequenceIndex);
}

bool isSoundSequenceStepActive()
{
    return sequenceRunning && sequenceStepActive;
}

unsigned long getSoundSequenceElapsedMs()
{
    if (!sequenceRunning || !sequenceStepActive)
    {
        return 0;
    }

    return millis() - sequenceStepStartedAt;
}

unsigned long getSoundSequenceRemainingMs()
{
    if (!sequenceRunning || !sequenceStepActive)
    {
        return 0;
    }

    size_t stepCount = 0;
    const SoundSequenceStep *steps = getSoundSequence(stepCount);
    if (stepCount == 0 || sequenceIndex >= stepCount)
    {
        return 0;
    }

    const unsigned long elapsedMs = getSoundSequenceElapsedMs();
    const unsigned long durationMs = steps[sequenceIndex].durationMs;
    if (elapsedMs >= durationMs)
    {
        return 0;
    }

    return durationMs - elapsedMs;
}

void updateDfPlayerScheduler()
{
    if (!sequenceRunning)
    {
        return;
    }

    size_t stepCount = 0;
    const SoundSequenceStep *steps = getSoundSequence(stepCount);
    if (stepCount == 0)
    {
        sequenceRunning = false;
        return;
    }

    if (!sequenceStepActive)
    {
        const SoundSequenceStep &step = steps[sequenceIndex];
        if (String(step.kind) == "pause")
        {
            stopUltrasound();
            dfPlayer.stop();
            Serial.print("DFPlayer sequence pause for ms: ");
            Serial.println(step.durationMs);
        }
        else if (String(step.kind) == "ultrasound")
        {
            if (dfPlayerReady)
            {
                dfPlayer.stop();
            }

            const UltrasoundSequenceDefinition *definition = findUltrasoundDefinitionByTag(step.tag);
            if (!definition)
            {
                Serial.print("DFPlayer sequence unknown ultrasound tag: ");
                Serial.println(step.tag);
            }
            else
            {
                playResolvedUltrasound(*definition, step.durationMs);
            }
        }
        else
        {
            stopUltrasound();
            if (!dfPlayerReady)
            {
                Serial.println("DFPlayer sequence sound skipped: player not ready.");
            }

            const SoundDefinition *definition = findSoundDefinition(String(step.tag));
            if (!dfPlayerReady)
            {
                // keep timing consistent even if the player is unavailable
            }
            else if (!definition)
            {
                Serial.print("DFPlayer sequence unknown tag: ");
                Serial.println(step.tag);
            }
            else
            {
                playResolvedSound(*definition);
            }
        }

        sequenceStepStartedAt = millis();
        sequenceStepActive = true;
    }

    const SoundSequenceStep &activeStep = steps[sequenceIndex];
    if (millis() - sequenceStepStartedAt < activeStep.durationMs)
    {
        return;
    }

    sequenceIndex = (sequenceIndex + 1) % stepCount;
    sequenceStepActive = false;
}
