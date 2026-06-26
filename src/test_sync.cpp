#include <iostream>
#include <windows.h>
#include "mic_capture.h"
#include "pulse_generator.h"
#include "pulse_detector.h"
#include "audio_device.h"
#include "logger.h"

// ─── Test 1: mic init ────────────────────────────────────────────────────────
bool TestMicInit() {
    std::cout << "\n[TEST 1] Mic initialization...\n";
    try {
        MicCapture mic;
        std::cout << "[PASS] MicCapture initialized\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[FAIL] MicCapture init threw: " << e.what() << "\n";
        return false;
    }
}

// ─── Test 2: pulse generation ─────────────────────────────────────────────────
bool TestPulseGenerate() {
    std::cout << "\n[TEST 2] Pulse generation...\n";
    try {
        AudioBuffer pulse = PulseGenerator::Generate(17000.0f, 200.0f, 44100);

        if (pulse.frameCount == 0) {
            std::cout << "[FAIL] Pulse has 0 frames\n";
            return false;
        }
        if (pulse.data.empty()) {
            std::cout << "[FAIL] Pulse data is empty\n";
            return false;
        }

        std::cout << "[PASS] Pulse generated — "
                  << pulse.frameCount << " frames, "
                  << pulse.sampleRate << "Hz, "
                  << pulse.channels << "ch\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[FAIL] PulseGenerator threw: " << e.what() << "\n";
        return false;
    }
}

// ─── Test 3: send pulse → mic receives it ─────────────────────────────────────
bool TestPulseSendReceive() {
    std::cout << "\n[TEST 3] Send pulse → mic receive...\n";

    try {
        // get first BT speaker
        AudioDevice audioDevice;
        auto speakers = audioDevice.GetBluetoothSpeakers();

        if (speakers.empty()) {
            std::cout << "[SKIP] No Bluetooth speakers found — plug one in and retry\n";
            return false;
        }

        std::cout << "  Using speaker: " << speakers[0].name << "\n";

        // generate pulse
        AudioBuffer pulse = PulseGenerator::Generate(17000.0f, 300.0f, 44100);
        std::cout << "  Pulse ready: 17000Hz, 300ms\n";

        // start mic capture first (captures for 1500ms total)
        MicCapture mic;
        std::cout << "  Mic capture starting...\n";

        // fire pulse then immediately capture
        // small thread to send pulse while main thread captures
        HANDLE pulseThread = CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
            auto* data = reinterpret_cast<std::pair<AudioDevice*, BluetoothSpeaker*>*>(param);
            AudioBuffer p = PulseGenerator::Generate(17000.0f, 300.0f, 44100);
            Sleep(200);  // let mic settle first
            data->first->SendAudioToSpeaker(*data->second, p);
            return 0;
        }, new std::pair<AudioDevice*, BluetoothSpeaker*>(&audioDevice, &speakers[0]), 0, nullptr);

        // capture 1500ms of room audio
        AudioBuffer micBuffer = mic.Capture(1500);
        WaitForSingleObject(pulseThread, INFINITE);
        CloseHandle(pulseThread);

        std::cout << "  Captured " << micBuffer.frameCount << " mic frames\n";

        // detect
        PulseDetector detector;
        auto result = detector.DetectFirstArrival(micBuffer, 17000.0f, 0.02f);

        if (!result.has_value()) {
            std::cout << "[FAIL] Pulse not detected in mic buffer\n";
            std::cout << "  → Try lowering threshold, or move mic closer to speaker\n";
            return false;
        }

        std::cout << "[PASS] Pulse detected!\n"
                  << "  timestamp : " << result->timestampMs << "ms\n"
                  << "  amplitude : " << result->amplitude   << "\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[FAIL] TestPulseSendReceive threw: " << e.what() << "\n";
        return false;
    }
}

// ─── runner ───────────────────────────────────────────────────────────────────
int main() {
    std::cout << "========================================\n";
    std::cout << "  bluetooth_sync — hardware tests\n";
    std::cout << "========================================\n";

    int passed = 0;
    int total  = 3;

    if (TestMicInit())          ++passed;
    if (TestPulseGenerate())    ++passed;
    if (TestPulseSendReceive()) ++passed;

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << passed << "/" << total << " passed\n";
    std::cout << "========================================\n";

    return passed == total ? 0 : 1;
}