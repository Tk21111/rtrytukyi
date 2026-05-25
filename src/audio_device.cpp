#include "audio_device.h"
#include "hresult_utils.h"
#include "logger.h"
#include <sstream>
#include <algorithm>
#include <cwctype>

namespace {
std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) {
        std::string fallback;
        fallback.reserve(value.size());
        for (wchar_t c : value) {
            fallback.push_back(c <= 0x7f ? static_cast<char>(c) : '?');
        }
        return fallback;
    }

    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}

bool ContainsBluetoothMarker(std::wstring value) {
    std::transform(value.begin(), value.end(),
                   value.begin(), [](wchar_t c) { return std::towlower(c); });

    return value.find(L"bth") != std::wstring::npos ||
           value.find(L"bluetooth") != std::wstring::npos;
}
}

AudioDevice::AudioDevice() : deviceEnumerator(nullptr), comInitialized(false) {
    try {
        LOG_INFO("Initializing Audio Device Manager");
        
        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            ThrowIfFailed(hr, "Failed to initialize COM");
        }
        comInitialized = true;
        
        // Create device enumerator
        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&deviceEnumerator
        );
        
        ThrowIfFailed(hr, "Failed to create device enumerator");
        
        LOG_INFO("Audio Device Manager initialized successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("AudioDevice initialization failed: " + std::string(e.what()));
        throw;
    }
}

AudioDevice::~AudioDevice() {
    if (deviceEnumerator) {
        deviceEnumerator->Release();
    }
    
    if (comInitialized) {
        CoUninitialize();
    }
    
    LOG_INFO("Audio Device Manager shutdown");
}

bool AudioDevice::IsBluetoothDevice(IMMDevice* device) {
    try {
        IPropertyStore* props = nullptr;
        HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
        if (LogIfFailed(hr, "Failed to open device property store")) return false;
        
        bool isBluetooth = false;
        PROPVARIANT var;
        PropVariantInit(&var);
        
        // 1. Check InstanceId for Bluetooth indicators (bthenum, bthhf, bthle, etc.)
        hr = props->GetValue(PKEY_Device_InstanceId, &var);
        LogIfFailed(hr, "Failed to read device instance id");
        if (SUCCEEDED(hr) && var.vt == VT_LPWSTR) {
            std::wstring instanceId(var.pwszVal);

            if (ContainsBluetoothMarker(instanceId)) {
                isBluetooth = true;
            }
        } else if (SUCCEEDED(hr)) {
            LOG_INFO("Device instance id is not a string: expected VT_LPWSTR, got " +
                     PropVariantTypeToString(var.vt) +
                     ". HRESULT succeeded, so this property is missing or empty on this endpoint.");
        }
        PropVariantClear(&var);

        // 2. Fallback: Check the endpoint ID if InstanceId didn't match.
        if (!isBluetooth) {
            LPWSTR deviceId = nullptr;
            hr = device->GetId(&deviceId);
            LogIfFailed(hr, "Failed to get endpoint id for Bluetooth fallback");
            if (SUCCEEDED(hr) && deviceId) {
                std::wstring endpointId(deviceId);
                isBluetooth = ContainsBluetoothMarker(endpointId);
                CoTaskMemFree(deviceId);
            }
        }

        // 3. Fallback: Check FriendlyName if IDs didn't match
        if (!isBluetooth) {
            hr = props->GetValue(PKEY_Device_FriendlyName, &var);
            LogIfFailed(hr, "Failed to read device friendly name for Bluetooth fallback");
            if (SUCCEEDED(hr) && var.vt == VT_LPWSTR) {
                std::wstring friendlyName(var.pwszVal);
                
                if (ContainsBluetoothMarker(friendlyName)) {
                    isBluetooth = true;
                }
            } else if (SUCCEEDED(hr)) {
                LOG_INFO("Device friendly name is not a string: expected VT_LPWSTR, got " +
                         PropVariantTypeToString(var.vt));
            }
            PropVariantClear(&var);
        }
        
        props->Release();
        return isBluetooth;
    }
    catch (...) {
        LOG_ERROR("Exception in IsBluetoothDevice");
    }
    
    return false;
}

std::string AudioDevice::GetDeviceName(IMMDevice* device) {
    try {
        IPropertyStore* props = nullptr;
        HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
        if (LogIfFailed(hr, "Failed to open device property store for name")) return "Unknown Device";
        
        PROPVARIANT varName;
        PropVariantInit(&varName);
        
        hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
        LogIfFailed(hr, "Failed to read device friendly name");
        if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
            std::wstring wname(varName.pwszVal);
            std::string name = WideToUtf8(wname);
            
            PropVariantClear(&varName);
            props->Release();
            return name;
        } else if (SUCCEEDED(hr)) {
            LOG_INFO("Device friendly name is not a string: expected VT_LPWSTR, got " +
                     PropVariantTypeToString(varName.vt));
        }
        
        PropVariantClear(&varName);
        props->Release();
    }
    catch (...) {
        LOG_ERROR("Exception in GetDeviceName");
    }
    
    return "Unknown Device";
}

bool AudioDevice::IsDeviceReady(IMMDevice* device) {
    try {
        DWORD state;
        HRESULT hr = device->GetState(&state);
        
        if (LogIfFailed(hr, "Failed to get device state")) {
            return false;
        }
        
        return (state == DEVICE_STATE_ACTIVE);
    }
    catch (...) {
        LOG_ERROR("Exception in IsDeviceReady");
        return false;
    }
}

std::vector<BluetoothSpeaker> AudioDevice::GetBluetoothSpeakers() {
    std::vector<BluetoothSpeaker> speakers;
    
    try {
        LOG_INFO("Enumerating Bluetooth speakers...");
        
        IMMDeviceCollection* deviceCollection = nullptr;
        HRESULT hr = deviceEnumerator->EnumAudioEndpoints(
            eRender,
            DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED,
            &deviceCollection
        );
        
        ThrowIfFailed(hr, "Failed to enumerate audio endpoints");
        
        UINT deviceCount = 0;
        hr = deviceCollection->GetCount(&deviceCount);
        if (LogIfFailed(hr, "Failed to get audio endpoint count")) {
            deviceCollection->Release();
            return speakers;
        }
        
        LOG_INFO("Found " + std::to_string(deviceCount) + " total audio devices");
        
        for (UINT i = 0; i < deviceCount; ++i) {
            IMMDevice* device = nullptr;
            hr = deviceCollection->Item(i, &device);
            
            if (LogIfFailed(hr, "Failed to get audio endpoint item")) {
                continue;
            }

            if (device) {

                if (IsBluetoothDevice(device)) {
                    BluetoothSpeaker speaker;
                    speaker.device = device;
                    speaker.name = GetDeviceName(device);
                    speaker.isActive = IsDeviceReady(device);
                    
                    // Get device ID
                    LPWSTR deviceId = nullptr;
                    hr = device->GetId(&deviceId);
                    LogIfFailed(hr, "Failed to get device id");
                    if (SUCCEEDED(hr)) {
                        std::wstring wid(deviceId);
                        speaker.id = WideToUtf8(wid);
                        CoTaskMemFree(deviceId);
                    }
                    
                    speakers.push_back(speaker);
                    
                    std::stringstream ss;
                    ss << "Found Bluetooth speaker: " << speaker.name 
                       << " (Active: " << (speaker.isActive ? "Yes" : "No") << ")";
                    LOG_INFO(ss.str());
                }
                else {
                    device->Release();
                }
            }
        }
        
        deviceCollection->Release();
        
        LOG_INFO("Total Bluetooth speakers found: " + std::to_string(speakers.size()));
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to enumerate Bluetooth speakers: " + std::string(e.what()));
    }
    
    return speakers;
}

bool AudioDevice::SendAudioToSpeaker(const BluetoothSpeaker& speaker, 
                                    const AudioBuffer& buffer) {
    try {
        LOG_INFO("Sending audio to speaker: " + speaker.name);
        
        if (!speaker.isActive) {
            throw std::runtime_error("Speaker is not active");
        }
        
        IAudioClient* audioClient = nullptr;
        HRESULT hr = speaker.device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            (void**)&audioClient
        );
        
        ThrowIfFailed(hr, "Failed to activate audio client");
        
        // Get device format
        WAVEFORMATEX* deviceFormat = nullptr;
        hr = audioClient->GetMixFormat(&deviceFormat);
        if (LogIfFailed(hr, "Failed to get mix format")) {
            audioClient->Release();
            throw std::runtime_error("Failed to get mix format");
        }
        
        LOG_INFO("Device format - Rate: " + std::to_string(deviceFormat->nSamplesPerSec) +
                "Hz, Channels: " + std::to_string(deviceFormat->nChannels));
        
        // Initialize audio client
        hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            10000000,  // 1 second buffer
            0,
            deviceFormat,
            nullptr
        );
        
        if (LogIfFailed(hr, "Failed to initialize audio client")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to initialize audio client");
        }
        
        // Get buffer size
        UINT32 bufferFrameCount;
        hr = audioClient->GetBufferSize(&bufferFrameCount);
        if (LogIfFailed(hr, "Failed to get audio client buffer size")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to get audio client buffer size");
        }
        
        // Get render client
        IAudioRenderClient* renderClient = nullptr;
        hr = audioClient->GetService(
            __uuidof(IAudioRenderClient),
            (void**)&renderClient
        );
        
        if (LogIfFailed(hr, "Failed to get render client")) {
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to get render client");
        }
        
        // Start playback
        hr = audioClient->Start();
        if (LogIfFailed(hr, "Failed to start audio playback")) {
            renderClient->Release();
            CoTaskMemFree(deviceFormat);
            audioClient->Release();
            throw std::runtime_error("Failed to start audio playback");
        }
        LOG_INFO("Playback started");
        
        // Send audio data in chunks
        uint32_t framesWritten = 0;
        while (framesWritten < buffer.frameCount) {
            UINT32 numFramesPadding;
            hr = audioClient->GetCurrentPadding(&numFramesPadding);
            if (LogIfFailed(hr, "Failed to get current audio padding")) {
                break;
            }
            
            UINT32 numFramesAvailable = bufferFrameCount - numFramesPadding;
            UINT32 framesToWrite = (std::min)(numFramesAvailable, buffer.frameCount - framesWritten);
            
            if (framesToWrite > 0) {
                BYTE* data;
                hr = renderClient->GetBuffer(framesToWrite, &data);
                LogIfFailed(hr, "Failed to get render buffer");
                
                if (SUCCEEDED(hr)) {
                    // Copy audio data
                    float* floatData = reinterpret_cast<float*>(data);
                    size_t sampleOffset = framesWritten * buffer.channels;
                    
                    for (UINT32 i = 0; i < framesToWrite * buffer.channels; ++i) {
                        floatData[i] = buffer.data[sampleOffset + i];
                    }
                    
                    hr = renderClient->ReleaseBuffer(framesToWrite, 0);
                    if (LogIfFailed(hr, "Failed to release render buffer")) {
                        break;
                    }
                    framesWritten += framesToWrite;
                }
            }
            
            Sleep(10);  // Small delay to prevent busy-wait
        }
        
        // Wait for playback to finish
        Sleep((DWORD)((buffer.frameCount / (float)buffer.sampleRate) * 1000) + 100);
        
        hr = audioClient->Stop();
        LogIfFailed(hr, "Failed to stop audio playback");
        LOG_INFO("Playback completed");
        
        // Cleanup
        renderClient->Release();
        CoTaskMemFree(deviceFormat);
        audioClient->Release();
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to send audio to speaker: " + std::string(e.what()));
        return false;
    }
}
