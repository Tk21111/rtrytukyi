#include "pulse_generator.h"
#include "logger.h"
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

AudioBuffer PulseGenerator::Generate(
    float    frequency,
    float    durationMs,
    uint32_t sampleRate)
{
    AudioBuffer buffer{};
    buffer.sampleRate = sampleRate;
    buffer.channels   = 1;  // mono pulse, speaker handles upmix
    buffer.frameCount = static_cast<uint32_t>((sampleRate * durationMs) / 1000.0f);
    buffer.data.resize(buffer.frameCount);

    float angularFreq = 2.0f * PI * frequency / static_cast<float>(sampleRate);

    // fade in/out over 10ms to avoid click artifacts
    uint32_t fadeSamples = static_cast<uint32_t>((sampleRate * 10) / 1000);

    for (uint32_t i = 0; i < buffer.frameCount; ++i) {
        float sample = std::sin(angularFreq * static_cast<float>(i));

        // fade in
        if (i < fadeSamples) {
            sample *= static_cast<float>(i) / static_cast<float>(fadeSamples);
        }
        // fade out
        else if (i >= buffer.frameCount - fadeSamples) {
            uint32_t fadePos = buffer.frameCount - i;
            sample *= static_cast<float>(fadePos) / static_cast<float>(fadeSamples);
        }

        buffer.data[i] = sample * 0.5f;  // 50% amplitude, don't blast the speakers
    }

    LOG_INFO("PulseGenerator: generated " + std::to_string(frequency) +
             "Hz pulse, " + std::to_string(buffer.frameCount) + " frames");

    return buffer;
}