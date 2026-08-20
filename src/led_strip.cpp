#include "led_strip.h"

#include <Adafruit_NeoPixel.h>

namespace
{
constexpr uint8_t LED_STRIP_PIN = 10;
constexpr uint16_t LED_STRIP_COUNT = 8; // adjust to match your strip length

Adafruit_NeoPixel strip(LED_STRIP_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);
} // namespace

void initLedStrip()
{
    strip.begin();
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
}

void updateLedStrip(LedStripState state)
{
    static LedStripState lastState = LedStripState::IDLE;

    if (state == lastState)
        return;

    lastState = state;

    switch (state)
    {
    case LedStripState::SOUND:
        strip.fill(strip.Color(255, 255, 255)); // white
        break;
    case LedStripState::ULTRASOUND:
        strip.fill(strip.Color(0, 0, 255)); // blue
        break;
    case LedStripState::PAUSE:
        strip.fill(strip.Color(0, 255, 0)); // green
        break;
    case LedStripState::IDLE:
    default:
        strip.fill(0); // off
        break;
    }
    strip.show();
}
