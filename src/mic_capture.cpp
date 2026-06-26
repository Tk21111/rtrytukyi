#include "mic_capture.h"
#include "hresult_utils.h"
#include "logger.h"
#include <stdexcept>
#include <vector>

MicCapture::MicCapture()
    : deviceEnumerator(nullptr), micDevice(nullptr), comInitialized(false) {
    Initialize();
}

MicCapture::~MicCapture() {
    Cleanup();
}

void MicCapture::Initialize() {
    LOG_INFO("Initializing MicCapture");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        ThrowIfFailed(hr, "MicCapture: Failed to initialize COM");
    }
    comInitialized = true;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator
    );
    ThrowIfFailed(hr, "MicCapture: Failed to create device enumerator");

    // ==========================================
    // NEW: Enumerate and loop through all mics
    // ==========================================
    IMMDeviceCollection* deviceCollection = nullptr;
    
    // Pass DEVICE_STATEMASK_ALL to get everything (active, disabled, unplugged)
    // Alternatively, passing DEVICE_STATE_ACTIVE here filters the list natively.
    hr = deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATEMASK_ALL, &deviceCollection);
    ThrowIfFailed(hr, "MicCapture: Failed to enumerate audio endpoints");

    UINT deviceCount = 0;
    hr = deviceCollection->GetCount(&deviceCount);
    ThrowIfFailed(hr, "MicCapture: Failed to get device count");

    LOG_INFO("MicCapture: Found " + std::to_string(deviceCount) + " capture device(s).");

    for (UINT i = 0; i < deviceCount; ++i) {
        IMMDevice* pDevice = nullptr;
        hr = deviceCollection->Item(i, &pDevice);
        if (FAILED(hr)) continue;

        DWORD state = 0;
        hr = pDevice->GetState(&state);
        
        // Check if the device is enabled and active
        if (SUCCEEDED(hr) && (state & DEVICE_STATE_ACTIVE)) {
            micDevice = pDevice; // Keep this device
            LOG_INFO("MicCapture: Selected active microphone at index " + std::to_string(i));
            break; // Stop looping once we find a good one
        } else {
            // Not active, release the COM object and check the next one
            pDevice->Release();
        }
    }

    // Clean up the collection
    deviceCollection->Release();

    // Verify we actually found one
    if (!micDevice) {
        throw std::runtime_error("MicCapture: No active and enabled microphones found on the system.");
    }
    // ==========================================

    LOG_INFO("MicCapture initialized successfully");
}

void MicCapture::Cleanup() {
    if (micDevice)        { micDevice->Release();        micDevice = nullptr; }
    if (deviceEnumerator) { deviceEnumerator->Release(); deviceEnumerator = nullptr; }
    if (comInitialized)   { CoUninitialize();            comInitialized = false; }
    LOG_INFO("MicCapture shutdown");
}

AudioBuffer MicCapture::Capture(uint32_t durationMs) {
    AudioBuffer result{};

    try {
        LOG_INFO("MicCapture: capturing for " + std::to_string(durationMs) + "ms");

        IAudioClient* audioClient = nullptr;
        HRESULT hr = micDevice->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            (void**)&audioClient
        );
        ThrowIfFailed(hr, "MicCapture: Failed to activate audio client");

        WAVEFORMATEX* deviceFormat = nullptr;
        hr = audioClient->GetMixFormat(&deviceFormat);
        if (LogIfFailed(hr, "MicCapture: Failed to get mix format")) {
            audioClient->Release();
            throw std::runtime_error("Failed to get mix format");
        }

        hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            10000000,   // 1 second buffer in 100ns units
            0,
            deviceFormat,
            nullptr
        );
        if (LogIfFailed(hr, "MicCapture: Failed to initialize audio client")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to initialize audio client");
        }

        IAudioCaptureClient* captureClient = nullptr;
        hr = audioClient->GetService(
            __uuidof(IAudioCaptureClient),
            (void**)&captureClient
        );
        if (LogIfFailed(hr, "MicCapture: Failed to get capture client")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to get capture client");
        }

        result.sampleRate = deviceFormat->nSamplesPerSec;
        result.channels   = deviceFormat->nChannels;

        hr = audioClient->Start();
        ThrowIfFailed(hr, "MicCapture: Failed to start capture");

        uint32_t totalFramesNeeded = (result.sampleRate * durationMs) / 1000;
        uint32_t framesCollected   = 0;
        result.data.reserve(totalFramesNeeded * result.channels);

        while (framesCollected < totalFramesNeeded) {
            Sleep(10);

            UINT32  packetLength = 0;
            hr = captureClient->GetNextPacketSize(&packetLength);
            if (LogIfFailed(hr, "MicCapture: GetNextPacketSize failed")) break;

            while (packetLength > 0) {
                BYTE*  pData        = nullptr;
                UINT32 numFrames    = 0;
                DWORD  flags        = 0;

                hr = captureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
                if (LogIfFailed(hr, "MicCapture: GetBuffer failed")) break;

                bool isSilent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                float* floatData = reinterpret_cast<float*>(pData);

                for (UINT32 i = 0; i < numFrames * result.channels; ++i) {
                    result.data.push_back(isSilent ? 0.0f : floatData[i]);
                }

                captureClient->ReleaseBuffer(numFrames);
                framesCollected += numFrames;

                hr = captureClient->GetNextPacketSize(&packetLength);
                if (LogIfFailed(hr, "MicCapture: GetNextPacketSize failed")) break;
            }
        }

        audioClient->Stop();
        result.frameCount = framesCollected;

        LOG_INFO("MicCapture: captured " + std::to_string(framesCollected) + " frames");

        captureClient->Release();
        CoTaskMemFree(deviceFormat);
        audioClient->Release();
    }
    catch (const std::exception& e) {
        LOG_ERROR("MicCapture::Capture failed: " + std::string(e.what()));
    }

    return result;
}