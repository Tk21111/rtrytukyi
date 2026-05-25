#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct AudioBuffer {
    std::vector<float> data;
    uint32_t sampleRate;
    uint16_t channels;
    uint32_t frameCount;
};

class AudioFile {
public:
    // Load WAV file from path and return audio buffer
    static AudioBuffer LoadWavFile(const std::string& filepath);
    
private:
    // WAV file header structures
    struct WavHeader {
        char riff[4];           // "RIFF"
        uint32_t fileSize;
        char wave[4];           // "WAVE"
    };
    
    struct WavFormat {
        char fmt[4];            // "fmt "
        uint32_t fmtSize;
        uint16_t audioFormat;   // 1 = PCM
        uint16_t channels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
    };
    
    struct WavData {
        char data[4];           // "data"
        uint32_t dataSize;
    };
};