#pragma once

#include <Arduino.h>

struct SoundDefinition
{
    const char *tag;
    const char *buttonLabel;
    const char *description;
    uint8_t folder;
    uint8_t track;
};

struct SoundSequenceStep
{
    const char *kind;
    const char *tag;
    uint32_t durationMs;
    const char *description;
};

struct UltrasoundSequenceDefinition
{
    const char *tag;
    const char *buttonLabel;
    const char *description;
    uint32_t minFrequencyHz;
    uint32_t maxFrequencyHz;
    bool randomMode;
};

constexpr size_t MAX_SOUND_SEQUENCE_STEPS = 12;

// Define sound definitions https://pixabay.com/de/sound-effects
// tag: unique identifier for the sound, used in the web interface
// buttonLabel: label to show on the button in the web interface
// description: description of the sound, can be shown in the web interface
// folder: the folder number on the SD card where the sound file is located
// track: the track number within the folder to play

constexpr SoundDefinition SOUND_DEFINITIONS[] = {
    {"dogbell", "Hundegeb\u00e4ll", "Hunde Geb\u00e4ll", 1, 1},
    {"dog", "Hunde", "Hunde Kurzes Gebell", 1, 2},
    {"lion", "L\u00f6we", "L\u00f6wen Gebr\u00fcll", 2, 1},
    {"wolf", "Wolf", "Wolf Geh\u00e4ule", 2, 2},
    {"diesel", "Diesel Motor", "Diesel Motor sound", 3, 1},
    {"engine", "Engine", "Start Engine", 3, 2},
    {"motor", "Motor", "Motor Sound", 3, 3},
    {"stop", "Stop", "Stop current playback", 0, 0},
};

constexpr SoundSequenceStep DEFAULT_SOUND_SEQUENCE[] = {
    {"sound", "dogbell", 4000, "Play dog bark ambience"},
    {"pause", "", 1500, "Pause after dog bark"},
    {"sound", "lion", 5000, "Play lion roar"},
    {"pause", "", 2000, "Pause after lion"},
    {"sound", "diesel", 4500, "Play diesel motor"},
    {"pause", "", 2500, "Pause after diesel"},
    {"sound", "wolf", 5000, "Play wolf howl"},
    {"pause", "", 3000, "Pause after wolf"},
};

constexpr UltrasoundSequenceDefinition ULTRASOUND_SEQUENCE_DEFINITIONS[] = {
    {"us-4k", "Ultrasound 4 kHz", "Fixed ultrasound at 4 kHz", 4000, 4000, false},
    {"us-8k", "Ultrasound 8 kHz", "Fixed ultrasound at 8 kHz", 8000, 8000, false},
    {"us-12k", "Ultrasound 12 kHz", "Fixed ultrasound at 12 kHz", 12000, 12000, false},
    {"us-24k", "Ultrasound 24 kHz", "Fixed ultrasound at 24 kHz", 24000, 24000, false},
    {"us-36k", "Ultrasound 36 kHz", "Fixed ultrasound at 36 kHz", 36000, 36000, false},
    {"us-random-8k-36k", "Ultrasound random 8-36 kHz", "Random ultrasound between 8 kHz and 36 kHz", 8000, 36000, true},
};
