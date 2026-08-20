#pragma once

enum class LedStripState
{
    IDLE,
    SOUND,
    ULTRASOUND,
    PAUSE
};

void initLedStrip();
void updateLedStrip(LedStripState state);
