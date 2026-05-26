#include "audio_file.h"
#include "logger.h"
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <array>

namespace {
struct ChunkHeader {
    char id[4];
    uint32_t size;
};

std::string ChunkIdToString(const char id[4]) {
    return std::string(id, 4);
}

void SkipChunkData(std::ifstream& file, uint32_t size) {
    file.seekg(size + (size % 2), std::ios::cur);
}

void SkipBytes(std::ifstream& file, uint32_t size) {
    file.seekg(size, std::ios::cur);
}
}

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
        
        WavFormat format = {};
        uint32_t dataSize = 0;
        std::streampos dataOffset = 0;
        bool foundFormat = false;
        bool foundData = false;

        while (file && (!foundFormat || !foundData)) {
            ChunkHeader chunk = {};
            file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
            if (file.gcount() == 0) {
                break;
            }
            if (file.gcount() != sizeof(chunk)) {
                throw std::runtime_error("Invalid WAV chunk header");
            }

            const std::string chunkId = ChunkIdToString(chunk.id);

            if (chunkId == "fmt ") {
                if (chunk.size < 16) {
                    throw std::runtime_error("Invalid WAV format chunk size");
                }

                format.fmt[0] = 'f';
                format.fmt[1] = 'm';
                format.fmt[2] = 't';
                format.fmt[3] = ' ';
                format.fmtSize = chunk.size;
                file.read(reinterpret_cast<char*>(&format.audioFormat), sizeof(WavFormat) - 8);
                if (!file) {
                    throw std::runtime_error("Failed to read WAV format chunk");
                }

                if (chunk.size > 16) {
                    SkipBytes(file, chunk.size - 16);
                }
                if (chunk.size % 2) {
                    SkipBytes(file, 1);
                }

                foundFormat = true;
            }
            else if (chunkId == "data") {
                dataSize = chunk.size;
                dataOffset = file.tellg();
                SkipChunkData(file, chunk.size);
                foundData = true;
            }
            else {
                LOG_INFO("Skipping WAV chunk: " + chunkId + " (" + std::to_string(chunk.size) + " bytes)");
                SkipChunkData(file, chunk.size);
            }
        }

        if (!foundFormat) {
            throw std::runtime_error("Format chunk not found");
        }

        if (!foundData) {
            throw std::runtime_error("Data chunk not found");
        }

        if (format.audioFormat != 1 && format.audioFormat != 3) {
            throw std::runtime_error("Only PCM and IEEE float WAV formats supported");
        }

        buffer.sampleRate = format.sampleRate;
        buffer.channels = format.channels;

        std::stringstream ss;
        ss << "WAV Info - Rate: " << format.sampleRate
           << "Hz, Channels: " << format.channels
           << ", Bits: " << format.bitsPerSample;
        LOG_INFO(ss.str());

        file.clear();
        file.seekg(dataOffset);
        
        // Read audio data
        uint32_t sampleCount = dataSize / (format.bitsPerSample / 8);
        buffer.frameCount = sampleCount / format.channels;
        buffer.data.resize(sampleCount);
        
        if (format.audioFormat == 1 && format.bitsPerSample == 16) {
            // Read 16-bit PCM and convert to float
            std::vector<int16_t> rawData(sampleCount);
            file.read(reinterpret_cast<char*>(rawData.data()), dataSize);
            
            for (size_t i = 0; i < sampleCount; ++i) {
                buffer.data[i] = rawData[i] / 32768.0f;
            }
        }
        else if (format.audioFormat == 3 && format.bitsPerSample == 32) {
            // Read 32-bit float directly
            file.read(reinterpret_cast<char*>(buffer.data.data()), dataSize);
        }
        else {
            throw std::runtime_error("Unsupported WAV format: audioFormat=" +
                                   std::to_string(format.audioFormat) +
                                   ", bitsPerSample=" + std::to_string(format.bitsPerSample));
        }

        if (!file) {
            throw std::runtime_error("Failed to read WAV data");
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
