#include <iostream>
#include <windows.h>
#include "audio_device.h"
#include "audio_file.h"
#include "logger.h"

static AudioDevice g_audio;

struct PlayParams {
    BluetoothSpeaker* speaker;
    AudioBuffer*      buffer;
};

DWORD WINAPI PlayThread(LPVOID param) {
    auto* p = reinterpret_cast<PlayParams*>(param);
    g_audio.SendAudioToSpeaker(*p->speaker, *p->buffer);
    return 0;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  simultaneous playback test\n";
    std::cout << "========================================\n";

    // ── grab devices by name ──────────────────────────────
    IMMDevice* btDevice     = g_audio.GetSpeakerByName("Techpro");
    IMMDevice* normalDevice = g_audio.GetSpeakerByName("Realtek");

    if (!btDevice) {
        std::cout << "[FAIL] Techpro LED Party not found\n";
        return 1;
    }
    if (!normalDevice) {
        std::cout << "[FAIL] Realtek speaker not found\n";
        return 1;
    }

    std::cout << "[OK] BT     : " << g_audio.GetDeviceName(btDevice)     << "\n";
    std::cout << "[OK] Normal : " << g_audio.GetDeviceName(normalDevice)  << "\n";

    // ── wrap as BluetoothSpeaker (reusing your struct) ────
    BluetoothSpeaker btSpk;
    btSpk.device   = btDevice;
    btSpk.name     = g_audio.GetDeviceName(btDevice);
    btSpk.isActive = true;

    BluetoothSpeaker normalSpk;
    normalSpk.device   = normalDevice;
    normalSpk.name     = g_audio.GetDeviceName(normalDevice);
    normalSpk.isActive = true;

    // ── load WAV ──────────────────────────────────────────
    std::cout << "\nLoading test1.wav...\n";
    AudioBuffer buf;
    try {
        buf = AudioFile::LoadWavFile("test/sound/test1.wav");
    } catch (const std::exception& e) {
        std::cout << "[FAIL] Could not load wav: " << e.what() << "\n";
        return 1;
    }
    std::cout << "[OK] Loaded: " << buf.frameCount << " frames, "
              << buf.sampleRate << "Hz, "
              << buf.channels   << "ch\n";

    // ── fire both threads at same time ────────────────────
    std::cout << "\nPlaying on both speakers simultaneously...\n";

    PlayParams paramBT    { &btSpk,    &buf };
    PlayParams paramNormal{ &normalSpk, &buf };

    HANDLE tBT     = CreateThread(nullptr, 0, PlayThread, &paramBT,     0, nullptr);
    HANDLE tNormal = CreateThread(nullptr, 0, PlayThread, &paramNormal, 0, nullptr);

    WaitForSingleObject(tBT,     INFINITE);
    WaitForSingleObject(tNormal, INFINITE);
    CloseHandle(tBT);
    CloseHandle(tNormal);

    std::cout << "[DONE] Playback complete\n";

    btDevice->Release();
    normalDevice->Release();
    return 0;
}