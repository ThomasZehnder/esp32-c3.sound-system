#include "ultrasound_pwm.h"

namespace
{
    constexpr uint8_t ULTRASOUND_PIN = 1;
    constexpr uint8_t ULTRASOUND_CHANNEL = 0;
    constexpr uint8_t ULTRASOUND_RESOLUTION_BITS = 10;
    constexpr uint32_t ULTRASOUND_MIN_FREQUENCY_HZ = 500;
    constexpr uint32_t ULTRASOUND_MAX_FREQUENCY_HZ = 50000;
    constexpr uint32_t ULTRASOUND_DEFAULT_FREQUENCY_HZ = 5000;
    constexpr uint32_t ULTRASOUND_MAX_DUTY = (1U << ULTRASOUND_RESOLUTION_BITS) - 1U;
    constexpr uint32_t ULTRASOUND_MAX_EFFECTIVE_DUTY = ULTRASOUND_MAX_DUTY / 2U;

    constexpr uint32_t ULTRASOUND_RANDOM_STEP_MS = 500;

    bool ultrasoundAttached = false;
    bool ultrasoundPinAttached = false;
    bool ultrasoundPlaying = false;
    bool ultrasoundRandomMode = false;
    uint32_t ultrasoundStopAtMs = 0;
    uint32_t ultrasoundNextRandomStepAtMs = 0;
    uint32_t ultrasoundFrequencyHz = ULTRASOUND_DEFAULT_FREQUENCY_HZ;
    uint32_t ultrasoundRandomMinFrequencyHz = ULTRASOUND_DEFAULT_FREQUENCY_HZ;
    uint32_t ultrasoundRandomMaxFrequencyHz = ULTRASOUND_DEFAULT_FREQUENCY_HZ;
    uint8_t ultrasoundVolumePercent = 0;

    void attachUltrasoundPin();
    void releaseUltrasoundPin();

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

    bool configureUltrasoundOutput(uint32_t frequencyHz, uint8_t volumePercent)
    {
        const uint32_t clampedFrequencyHz = clampFrequency(frequencyHz);
        const uint8_t clampedVolumePercent = volumePercent > 100 ? 100 : volumePercent;
        const uint32_t duty = volumeToDuty(clampedVolumePercent);

        if (ledcSetup(ULTRASOUND_CHANNEL, clampedFrequencyHz, ULTRASOUND_RESOLUTION_BITS) == 0)
        {
            return false;
        }

        Serial.println("Ultrasound PWM frequency set to " + String(clampedFrequencyHz) + " Hz with duty " + String(duty) + "/" + String(ULTRASOUND_MAX_DUTY));

        attachUltrasoundPin();
        ledcWrite(ULTRASOUND_CHANNEL, duty);
        ultrasoundFrequencyHz = clampedFrequencyHz;
        ultrasoundVolumePercent = clampedVolumePercent;
        return true;
    }

    uint32_t randomFrequencyInRange(uint32_t minFrequencyHz, uint32_t maxFrequencyHz)
    {
        if (minFrequencyHz >= maxFrequencyHz)
        {
            return minFrequencyHz;
        }

        return static_cast<uint32_t>(random(static_cast<long>(minFrequencyHz), static_cast<long>(maxFrequencyHz + 1U)));
    }

    void attachUltrasoundPin()
    {
        if (ultrasoundPinAttached)
        {
            return;
        }
        Serial.println("Attaching ultrasound pin to LEDC channel...");  
        ledcAttachPin(ULTRASOUND_PIN, ULTRASOUND_CHANNEL);
        ultrasoundPinAttached = true;
    }

    void releaseUltrasoundPin()
    {
        if (ultrasoundPinAttached)
        {
            Serial.println("Detaching ultrasound pin from LEDC channel...");
            ledcDetachPin(ULTRASOUND_PIN);
            ultrasoundPinAttached = false;
        }

        pinMode(ULTRASOUND_PIN, INPUT_PULLDOWN);
        digitalWrite(ULTRASOUND_PIN, LOW);
    }

}

void initUltrasoundPwm()
{
    if (ultrasoundAttached)
    {
        return;
    }

    ultrasoundAttached = ledcSetup(ULTRASOUND_CHANNEL, ULTRASOUND_DEFAULT_FREQUENCY_HZ, ULTRASOUND_RESOLUTION_BITS) > 0;
    randomSeed(micros());
}

bool playUltrasound(uint32_t frequencyHz, uint8_t volumePercent, uint32_t playTimeMs)
{
    initUltrasoundPwm();
    if (!ultrasoundAttached)
    {
        return false;
    }

    if (!configureUltrasoundOutput(frequencyHz, volumePercent))
    {
        return false;
    }

    ultrasoundRandomMode = false;
    ultrasoundRandomMinFrequencyHz = ultrasoundFrequencyHz;
    ultrasoundRandomMaxFrequencyHz = ultrasoundFrequencyHz;
    ultrasoundPlaying = ultrasoundVolumePercent > 0 && playTimeMs > 0;
    ultrasoundStopAtMs = millis() + playTimeMs;
    ultrasoundNextRandomStepAtMs = 0;

    if (!ultrasoundPlaying)
    {
        stopUltrasound();
    }

    return true;
}

bool playRandomUltrasound(uint32_t minFrequencyHz, uint32_t maxFrequencyHz, uint8_t volumePercent, uint32_t playTimeMs)
{
    initUltrasoundPwm();
    if (!ultrasoundAttached)
    {
        return false;
    }

    const uint32_t clampedMinFrequencyHz = clampFrequency(minFrequencyHz);
    const uint32_t clampedMaxFrequencyHz = clampFrequency(maxFrequencyHz);
    const uint32_t randomMinFrequencyHz = clampedMinFrequencyHz < clampedMaxFrequencyHz ? clampedMinFrequencyHz : clampedMaxFrequencyHz;
    const uint32_t randomMaxFrequencyHz = clampedMinFrequencyHz < clampedMaxFrequencyHz ? clampedMaxFrequencyHz : clampedMinFrequencyHz;
    const uint32_t firstFrequencyHz = randomFrequencyInRange(randomMinFrequencyHz, randomMaxFrequencyHz);

    if (!configureUltrasoundOutput(firstFrequencyHz, volumePercent))
    {
        return false;
    }

    ultrasoundRandomMode = true;
    ultrasoundRandomMinFrequencyHz = randomMinFrequencyHz;
    ultrasoundRandomMaxFrequencyHz = randomMaxFrequencyHz;
    ultrasoundPlaying = ultrasoundVolumePercent > 0 && playTimeMs > 0;
    ultrasoundStopAtMs = millis() + playTimeMs;
    ultrasoundNextRandomStepAtMs = millis() + ULTRASOUND_RANDOM_STEP_MS;

    if (!ultrasoundPlaying)
    {
        stopUltrasound();
    }

    return true;
}

void stopUltrasound()
{
    if (!ultrasoundAttached)
    {
        return;
    }

    releaseUltrasoundPin();
    ultrasoundPlaying = false;
    ultrasoundRandomMode = false;
    ultrasoundStopAtMs = 0;
    ultrasoundNextRandomStepAtMs = 0;
    Serial.println("Ultrasound PWM stopped.");  
    ultrasoundVolumePercent = 0;
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
        return;
    }

    if (ultrasoundRandomMode && static_cast<int32_t>(millis() - ultrasoundNextRandomStepAtMs) >= 0)
    {
        const uint32_t nextFrequencyHz = randomFrequencyInRange(ultrasoundRandomMinFrequencyHz, ultrasoundRandomMaxFrequencyHz);
        configureUltrasoundOutput(nextFrequencyHz, ultrasoundVolumePercent);
        ultrasoundNextRandomStepAtMs = millis() + ULTRASOUND_RANDOM_STEP_MS;
    }
}

bool isUltrasoundPlaying()
{
    return ultrasoundPlaying;
}

bool isUltrasoundRandomMode()
{
    return ultrasoundRandomMode;
}

uint32_t getUltrasoundFrequencyHz()
{
    return ultrasoundFrequencyHz;
}

uint8_t getUltrasoundVolumePercent()
{
    return ultrasoundVolumePercent;
}

uint32_t getUltrasoundRemainingMs()
{
    if (!ultrasoundPlaying)
    {
        return 0;
    }

    const int32_t remainingMs = static_cast<int32_t>(ultrasoundStopAtMs - millis());
    return remainingMs > 0 ? static_cast<uint32_t>(remainingMs) : 0;
}

uint8_t getUltrasoundPin()
{
    return ULTRASOUND_PIN;
}

uint32_t getUltrasoundMinFrequencyHz()
{
    return ULTRASOUND_MIN_FREQUENCY_HZ;
}

uint32_t getUltrasoundMaxFrequencyHz()
{
    return ULTRASOUND_MAX_FREQUENCY_HZ;
}

uint32_t getUltrasoundRandomMinFrequencyHz()
{
    return ultrasoundRandomMinFrequencyHz;
}

uint32_t getUltrasoundRandomMaxFrequencyHz()
{
    return ultrasoundRandomMaxFrequencyHz;
}