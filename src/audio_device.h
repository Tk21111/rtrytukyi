#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include "audio_file.h"

struct BluetoothSpeaker {
    std::string id;
    std::string name;
    bool isActive;
    IMMDevice* device;
};

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();
    
    // Enumerate and return all Bluetooth speakers
    std::vector<BluetoothSpeaker> GetBluetoothSpeakers();
    
    // Send audio buffer to specific speaker
    bool SendAudioToSpeaker(const BluetoothSpeaker& speaker, 
                           const AudioBuffer& buffer);
    
private:
    // Check if device is Bluetooth
    bool IsBluetoothDevice(IMMDevice* device);
    
    // Get device friendly name
    std::string GetDeviceName(IMMDevice* device);
    
    // Check if device is active and ready
    bool IsDeviceReady(IMMDevice* device);
    
    IMMDeviceEnumerator* deviceEnumerator;
    bool comInitialized;
};