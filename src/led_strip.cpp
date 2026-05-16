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

void updateLedStrip(bool sequenceRunning)
{
    static bool lastSequenceRunning = false;

    if (sequenceRunning == lastSequenceRunning)
        return;

    lastSequenceRunning = sequenceRunning;

    if (sequenceRunning)
    {
        strip.fill(strip.Color(255, 255, 255));
    }
    else
    {
        strip.fill(0);
    }
    strip.show();
}
