#include "audio_file.h"
#include "logger.h"
#include <fstream>
#include <stdexcept>
#include <sstream>

AudioBuffer AudioFile::LoadWavFile(const std::string& filepath) {
    AudioBuffer buffer;
    
    try {
        LOG_INFO("Loading audio file: " + filepath);
        
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filepath);
        }
        
        // Read WAV header
        WavHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
        
        if (std::string(header.riff, 4) != "RIFF" || 
            std::string(header.wave, 4) != "WAVE") {
            throw std::runtime_error("Invalid WAV file format");
        }
        
        // Read format chunk
        WavFormat format;
        file.read(reinterpret_cast<char*>(&format), sizeof(WavFormat));
        
        if (std::string(format.fmt, 4) != "fmt ") {
            throw std::runtime_error("Invalid WAV format chunk");
        }
        
        if (format.audioFormat != 1) {
            throw std::runtime_error("Only PCM format supported");
        }
        
        buffer.sampleRate = format.sampleRate;
        buffer.channels = format.channels;
        
        std::stringstream ss;
        ss << "WAV Info - Rate: " << format.sampleRate 
           << "Hz, Channels: " << format.channels 
           << ", Bits: " << format.bitsPerSample;
        LOG_INFO(ss.str());
        
        // Find data chunk
        WavData dataChunk;
        file.read(reinterpret_cast<char*>(&dataChunk), sizeof(WavData));
        
        if (std::string(dataChunk.data, 4) != "data") {
            throw std::runtime_error("Data chunk not found");
        }
        
        // Read audio data
        uint32_t sampleCount = dataChunk.dataSize / (format.bitsPerSample / 8);
        buffer.frameCount = sampleCount / format.channels;
        buffer.data.resize(sampleCount);
        
        if (format.bitsPerSample == 16) {
            // Read 16-bit PCM and convert to float
            std::vector<int16_t> rawData(sampleCount);
            file.read(reinterpret_cast<char*>(rawData.data()), dataChunk.dataSize);
            
            for (size_t i = 0; i < sampleCount; ++i) {
                buffer.data[i] = rawData[i] / 32768.0f;
            }
        }
        else if (format.bitsPerSample == 32) {
            // Read 32-bit float directly
            file.read(reinterpret_cast<char*>(buffer.data.data()), dataChunk.dataSize);
        }
        else {
            throw std::runtime_error("Unsupported bit depth: " + 
                                   std::to_string(format.bitsPerSample));
        }
        
        file.close();
        
        ss.str("");
        ss << "Loaded " << buffer.frameCount << " frames ("
           << (buffer.frameCount / (float)buffer.sampleRate) << " seconds)";
        LOG_INFO(ss.str());
        
        return buffer;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to load audio file: " + std::string(e.what()));
        throw;
    }
}