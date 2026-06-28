#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "audio_file.h"

class MicCapture {
public:
    MicCapture();
    ~MicCapture();

    // capture audio for durationMs milliseconds, returns filled AudioBuffer
    AudioBuffer Capture(uint32_t durationMs);
    IMMDevice* FindMicByName(const std::string& nameContains);

private:
    IMMDeviceEnumerator* deviceEnumerator;
    IMMDevice*           micDevice;
    bool                 comInitialized;

    void Initialize();
    void Cleanup();
};