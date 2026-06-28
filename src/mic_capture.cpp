#include "mic_capture.h"
#include "hresult_utils.h"
#include "logger.h"
#include <stdexcept>
#include <vector>
#include <Functiondiscoverykeys_devpkey.h> //fix PKEY_Device_FriendlyName undefined // uuid lib prob
MicCapture::MicCapture()
    : deviceEnumerator(nullptr), micDevice(nullptr), comInitialized(false) {
    Initialize();
}

MicCapture::~MicCapture() {
    Cleanup();
}

IMMDevice* MicCapture::FindMicByName(const std::string& nameContains) {
    IMMDeviceCollection* collection = nullptr;
    HRESULT hr = deviceEnumerator->EnumAudioEndpoints(
        eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return nullptr;

    UINT count = 0;
    collection->GetCount(&count);
    LOG_INFO("MicCapture: scanning " + std::to_string(count) + " capture device(s)");

    IMMDevice* found = nullptr;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDevice = nullptr;
        collection->Item(i, &pDevice);
        if (!pDevice) continue;

        // get friendly name
        IPropertyStore* pProps = nullptr;
        pDevice->OpenPropertyStore(STGM_READ, &pProps);

        PROPVARIANT var;
        PropVariantInit(&var);
        pProps->GetValue(PKEY_Device_FriendlyName, &var);
        pProps->Release();

        std::wstring wname(var.pwszVal ? var.pwszVal : L"");
        PropVariantClear(&var);

        // convert to narrow
        std::string name(wname.begin(), wname.end());
        LOG_INFO("MicCapture: found device → " + name);

        if (name.find(nameContains) != std::string::npos) {
            LOG_INFO("MicCapture: selected → " + name);
            found = pDevice;
            break;
        }
        pDevice->Release();
    }

    collection->Release();
    return found;
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

    // hardcoded: Realtek mic
    micDevice = FindMicByName("Realtek");

    if (!micDevice) {
        throw std::runtime_error("MicCapture: Realtek microphone not found");
    }

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
            __uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
        ThrowIfFailed(hr, "MicCapture: Failed to activate audio client");

        IAudioClient2* audioClient2 = nullptr;
        if (SUCCEEDED(audioClient->QueryInterface(__uuidof(IAudioClient2), (void**)&audioClient2))) {
            AudioClientProperties props = {};
            props.cbSize    = sizeof(AudioClientProperties);
            props.bIsOffload = FALSE;
            props.eCategory  = AudioCategory_Other;
            props.Options    = AUDCLNT_STREAMOPTIONS_RAW;  // raw = no Windows FX
            HRESULT rawHr = audioClient2->SetClientProperties(&props);
            if (SUCCEEDED(rawHr)) {
                LOG_INFO("MicCapture: RAW mode enabled — Windows audio processing bypassed");
            } else {
                LOG_INFO("MicCapture: RAW mode not supported on this device, continuing with processing");
            }
            audioClient2->Release();
}

        WAVEFORMATEX* deviceFormat = nullptr;
        hr = audioClient->GetMixFormat(&deviceFormat);
        if (LogIfFailed(hr, "MicCapture: Failed to get mix format")) {
            audioClient->Release();
            throw std::runtime_error("Failed to get mix format");
        }

        hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, deviceFormat, nullptr);
        if (LogIfFailed(hr, "MicCapture: Failed to initialize audio client")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to initialize audio client");
        }

        IAudioCaptureClient* captureClient = nullptr;
        hr = audioClient->GetService(
            __uuidof(IAudioCaptureClient), (void**)&captureClient);
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
            UINT32 packetLength = 0;
            hr = captureClient->GetNextPacketSize(&packetLength);
            if (LogIfFailed(hr, "MicCapture: GetNextPacketSize failed")) break;

            while (packetLength > 0 && framesCollected < totalFramesNeeded) {
                BYTE*  pData     = nullptr;
                UINT32 numFrames = 0;
                DWORD  flags     = 0;

                hr = captureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
                if (LogIfFailed(hr, "MicCapture: GetBuffer failed")) break;

                bool   isSilent  = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
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