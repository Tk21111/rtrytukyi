#pragma once
#include <optional>
#include <cstdint>
#include "audio_file.h"  // reuses your AudioBuffer

struct DetectionResult {
    uint64_t timestampMs;   // QueryPerformanceCounter converted to ms
    float    amplitude;     // magnitude at target freq when detected
};

class PulseDetector {
public:
    PulseDetector(int fftWindowSize = 1024);
    ~PulseDetector();

    // returns first crossing timestamp, empty if not found
    std::optional<DetectionResult> DetectFirstArrival(
        const AudioBuffer& micBuffer,
        float targetFreq,
        float threshold
    );

private:
    int windowSize;

    float GetMagnitudeAtFreq(
        const float* samples,
        int count,
        float targetFreq,
        float sampleRate
    );
};