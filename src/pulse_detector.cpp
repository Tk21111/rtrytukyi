#include "pulse_detector.h"
#include "logger.h"
#include "kiss_fft.h"
#include <windows.h>
#include <cmath>
#include <vector>

PulseDetector::PulseDetector(int fftWindowSize) : windowSize(fftWindowSize) {
    LOG_INFO("PulseDetector initialized, window size: " + std::to_string(windowSize));
}

PulseDetector::~PulseDetector() {}

float PulseDetector::GetMagnitudeAtFreq(
    const float* samples,
    int count,
    float targetFreq,
    float sampleRate)
{
    kiss_fft_cfg cfg = kiss_fft_alloc(count, 0, nullptr, nullptr);
    if (!cfg) {
        LOG_ERROR("KissFFT alloc failed");
        return 0.0f;
    }

    // fill input
    std::vector<kiss_fft_cpx> in(count), out(count);
    for (int i = 0; i < count; ++i) {
        in[i].r = samples[i];
        in[i].i = 0.0f;
    }

    kiss_fft(cfg, in.data(), out.data());
    kiss_fft_free(cfg);

    // which bin is our target frequency?
    int targetBin = static_cast<int>(targetFreq * count / sampleRate);
    targetBin = std::min(targetBin, count / 2 - 1);

    float r = out[targetBin].r;
    float im = out[targetBin].i;
    return std::sqrt(r * r + im * im) / count;  // normalized magnitude
}

std::optional<DetectionResult> PulseDetector::DetectFirstArrival(
    const AudioBuffer& micBuffer,
    float targetFreq,
    float threshold)
{
    LOG_INFO("Scanning for pulse at " + std::to_string(targetFreq) + "Hz, threshold: " + std::to_string(threshold));

    int totalFrames = static_cast<int>(micBuffer.frameCount);
    int step        = windowSize / 2;
    float maxMagnitudeSeen = 0.0f;  // ← track this

    for (int offset = 0; offset + windowSize <= totalFrames; offset += step) {
        const float* window = micBuffer.data.data() + offset * micBuffer.channels;

        std::vector<float> mono(windowSize);
        for (int i = 0; i < windowSize; ++i) {
            mono[i] = window[i * micBuffer.channels];
        }

        float magnitude = GetMagnitudeAtFreq(
            mono.data(), windowSize,
            targetFreq, static_cast<float>(micBuffer.sampleRate)
        );

        if (magnitude > maxMagnitudeSeen) maxMagnitudeSeen = magnitude;

        if (magnitude >= threshold) {
            double offsetMs = (static_cast<double>(offset) / micBuffer.sampleRate) * 1000.0;
            DetectionResult result;
            result.timestampMs = static_cast<uint64_t>(offsetMs);
            result.amplitude   = magnitude;
            LOG_INFO("Pulse detected at " + std::to_string(offsetMs) +
                     "ms, magnitude: " + std::to_string(magnitude));
            return result;
        }
    }

    // ← tells you exactly how far off threshold you are
    LOG_INFO("No pulse detected. Max magnitude seen: " + std::to_string(maxMagnitudeSeen) +
             " vs threshold: " + std::to_string(threshold));
    std::cout << "  [DEBUG] max magnitude at " << (int)targetFreq 
              << "Hz: " << maxMagnitudeSeen << " (threshold: " << threshold << ")\n";
    return std::nullopt;
}

// PulseDetector detector;

// auto resultA = detector.DetectFirstArrival(micBuffer, 17000.0f, 0.05f);
// auto resultB = detector.DetectFirstArrival(micBuffer, 18000.0f, 0.05f);

// if (resultA && resultB) {
//     int64_t offset = (int64_t)resultA->timestampMs - (int64_t)resultB->timestampMs;
//     // offset > 0 means A is slower → delay A next time
// }