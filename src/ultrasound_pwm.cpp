#include "ultrasound_pwm.h"

namespace
{
constexpr uint8_t ULTRASOUND_PIN = 0;
constexpr uint8_t ULTRASOUND_CHANNEL = 0;
constexpr uint8_t ULTRASOUND_RESOLUTION_BITS = 10;
constexpr uint32_t ULTRASOUND_MIN_FREQUENCY_HZ = 5000;
constexpr uint32_t ULTRASOUND_MAX_FREQUENCY_HZ = 40000;
constexpr uint32_t ULTRASOUND_DEFAULT_FREQUENCY_HZ = 5000;
constexpr uint32_t ULTRASOUND_MAX_DUTY = (1U << ULTRASOUND_RESOLUTION_BITS) - 1U;
constexpr uint32_t ULTRASOUND_MAX_EFFECTIVE_DUTY = ULTRASOUND_MAX_DUTY / 2U;

bool ultrasoundAttached = false;
bool ultrasoundPlaying = false;
uint32_t ultrasoundStopAtMs = 0;

uint32_t clampFrequency(uint32_t frequencyHz)
{
    if (frequencyHz < ULTRASOUND_MIN_FREQUENCY_HZ)
    {
        return ULTRASOUND_MIN_FREQUENCY_HZ;
    }

    if (frequencyHz > ULTRASOUND_MAX_FREQUENCY_HZ)
    {
        return ULTRASOUND_MAX_FREQUENCY_HZ;
    }

    return frequencyHz;
}

uint32_t volumeToDuty(uint8_t volumePercent)
{
    const uint8_t clampedVolume = volumePercent > 100 ? 100 : volumePercent;
    return (ULTRASOUND_MAX_EFFECTIVE_DUTY * clampedVolume) / 100U;
}
}

void initUltrasoundPwm()
{
    if (ultrasoundAttached)
    {
        return;
    }

    ultrasoundAttached = ledcSetup(ULTRASOUND_CHANNEL, ULTRASOUND_DEFAULT_FREQUENCY_HZ, ULTRASOUND_RESOLUTION_BITS) > 0;
    if (ultrasoundAttached)
    {
        ledcAttachPin(ULTRASOUND_PIN, ULTRASOUND_CHANNEL);
        ledcWrite(ULTRASOUND_CHANNEL, 0);
    }
}

bool playUltrasound(uint32_t frequencyHz, uint8_t volumePercent, uint32_t playTimeMs)
{
    initUltrasoundPwm();
    if (!ultrasoundAttached)
    {
        return false;
    }

    const uint32_t clampedFrequencyHz = clampFrequency(frequencyHz);
    const uint32_t duty = volumeToDuty(volumePercent);

    if (ledcSetup(ULTRASOUND_CHANNEL, clampedFrequencyHz, ULTRASOUND_RESOLUTION_BITS) == 0)
    {
        return false;
    }

    ledcWrite(ULTRASOUND_CHANNEL, duty);
    ultrasoundPlaying = duty > 0 && playTimeMs > 0;
    ultrasoundStopAtMs = millis() + playTimeMs;
    return true;
}

void stopUltrasound()
{
    if (!ultrasoundAttached)
    {
        return;
    }

    ledcWrite(ULTRASOUND_CHANNEL, 0);
    ultrasoundPlaying = false;
    ultrasoundStopAtMs = 0;
}

void updateUltrasoundPwm()
{
    if (!ultrasoundPlaying)
    {
        return;
    }

    if (static_cast<int32_t>(millis() - ultrasoundStopAtMs) >= 0)
    {
        stopUltrasound();
    }
}