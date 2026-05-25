#include <iostream>
#include "logger.h"
#include "audio_device.h"
#include "audio_file.h"

int main() {
    try {
        LOG_INFO("=== Bluetooth Speaker Sync - Started ===");
        
        // Step 1: Initialize audio device manager
        AudioDevice audioDevice;
        
        // Step 2: Get all Bluetooth speakers
        auto speakers = audioDevice.GetBluetoothSpeakers();
        
        if (speakers.empty()) {
            LOG_ERROR("No Bluetooth speakers found!");
            return 1;
        }
        
        std::cout << "\n=== Found Bluetooth Speakers ===" << std::endl;
        for (size_t i = 0; i < speakers.size(); ++i) {
            std::cout << i + 1 << ". " << speakers[i].name 
                     << " [" << (speakers[i].isActive ? "ACTIVE" : "INACTIVE") << "]" 
                     << std::endl;
        }
        
        // Step 3: Load test audio file
        LOG_INFO("Loading test audio file...");
        AudioBuffer buffer = AudioFile::LoadWavFile("test/sound/test1.wav");
        
        // Step 4: Play on first active speaker
        for (const auto& speaker : speakers) {
            if (speaker.isActive) {
                std::cout << "\nPlaying on: " << speaker.name << std::endl;
                
                if (audioDevice.SendAudioToSpeaker(speaker, buffer)) {
                    LOG_INFO("Successfully played audio on " + speaker.name);
                } else {
                    LOG_ERROR("Failed to play audio on " + speaker.name);
                }
                
                break;  // Only play on first active speaker for now
            }
        }
        
        LOG_INFO("=== Bluetooth Speaker Sync - Finished ===");
        
        // Cleanup speaker devices
        for (auto& speaker : speakers) {
            if (speaker.device) {
                speaker.device->Release();
            }
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Fatal error: " + std::string(e.what()));
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        LOG_ERROR("Unknown fatal error occurred");
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}