#pragma once
#include <cstdint>
#include "audio_file.h"

class PulseGenerator {
public:
    // generates a sine wave pulse, ready for SendAudioToSpeaker
    static AudioBuffer Generate(
        float    frequency,   // e.g. 17000.0f
        float    durationMs,  // e.g. 200.0f
        uint32_t sampleRate   // e.g. 44100
    );
};