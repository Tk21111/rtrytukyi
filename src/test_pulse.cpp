#include <iostream>
#include <windows.h>
#include "audio_device.h"
#include "audio_file.h"
#include "mic_capture.h"
#include "pulse_generator.h"
#include "pulse_detector.h"
#include "logger.h"

// ── globals ───────────────────────────────────────────────────────────────────
static AudioDevice   g_audio;
static MicCapture    g_mic;
static PulseDetector g_detector;

// ── hardcoded speakers ────────────────────────────────────────────────────────
static BluetoothSpeaker g_normal;   // SK
static BluetoothSpeaker g_bt;       // Techpro

// ── log helpers ───────────────────────────────────────────────────────────────
void LogSent(const std::string& speakerName, float freq) {
    std::string msg = "[PULSE SENT] → " + speakerName +
                      " at " + std::to_string((int)freq) + "Hz";
    std::cout << msg << "\n";
    LOG_INFO(msg);
}

void LogRecv(float freq, const std::optional<DetectionResult>& result) {
    if (result.has_value()) {
        std::string msg = "[PULSE RECV] → " + std::to_string((int)freq) +
                          "Hz at " + std::to_string(result->timestampMs) +
                          "ms  amplitude: " + std::to_string(result->amplitude);
        std::cout << msg << "\n";
        LOG_INFO(msg);
    } else {
        std::string msg = "[PULSE MISS] → " + std::to_string((int)freq) +
                          "Hz not detected";
        std::cout << msg << "\n";
        LOG_INFO(msg);
    }
}

// ── thread helpers ────────────────────────────────────────────────────────────
struct SendPulseParams {
    BluetoothSpeaker* speaker;
    float             frequency;
    uint32_t          delayMs;
    HANDLE            startGate;
};

DWORD WINAPI SendPulseThread(LPVOID param) {
    auto* p = reinterpret_cast<SendPulseParams*>(param);
    WaitForSingleObject(p->startGate, INFINITE);  // ← wait at gate
    AudioBuffer pulse = PulseGenerator::Generate(p->frequency, 300.0f, 44100);
    LogSent(p->speaker->name, p->frequency);
    g_audio.SendAudioToSpeaker(*p->speaker, pulse);
    return 0;
}

struct SendMusicParams {
    BluetoothSpeaker* speaker;
    AudioBuffer*      music;
};

DWORD WINAPI SendMusicThread(LPVOID param) {
    auto* p = reinterpret_cast<SendMusicParams*>(param);
    LOG_INFO("Music started on " + p->speaker->name);
    g_audio.SendAudioToSpeaker(*p->speaker, *p->music);
    LOG_INFO("Music finished on " + p->speaker->name);
    return 0;
}

// ── send pulse → capture → detect (single speaker) ───────────────────────────
bool RunPulseCapture(
    BluetoothSpeaker& speaker,
    float             freq,
    uint32_t          captureMs,
    float             threshold = 0.02f)
{
    SendPulseParams params{ &speaker, freq, 200 };   // 200ms delay so mic settles

    HANDLE t = CreateThread(nullptr, 0, SendPulseThread, &params, 0, nullptr);
    AudioBuffer micBuf = g_mic.Capture(captureMs);
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);

    auto result = g_detector.DetectFirstArrival(micBuf, freq, threshold);
    LogRecv(freq, result);
    return result.has_value();
}

// ── setup ─────────────────────────────────────────────────────────────────────
bool Setup() {
    IMMDevice* normalDev = g_audio.GetSpeakerByName("Techpro");
    IMMDevice* btDev     = g_audio.GetSpeakerByName("Techpro");

    if (!normalDev) { std::cout << "[FAIL] Techpro speaker not found\n";  return false; }
    if (!btDev)     { std::cout << "[FAIL] SK speaker not found\n";  return false; }

    g_normal = { "", g_audio.GetDeviceName(normalDev), true, normalDev };
    g_bt     = { "", g_audio.GetDeviceName(btDev),     true, btDev     };

    std::cout << "[OK] Normal : " << g_normal.name << "\n";
    std::cout << "[OK] BT     : " << g_bt.name     << "\n";
    return true;
}

// ── TEST 1: pulse only, sequential ───────────────────────────────────────────
bool Test1_PulseOnly() {
    std::cout << "\n[TEST 1] Pulse only — no music\n";
    std::cout << "----------------------------------------\n";

    HANDLE startGate = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    SendPulseParams paramA{ &g_normal, 17000.0f, 0, startGate };
    SendPulseParams paramB{ &g_bt,     18000.0f, 0, startGate };

    HANDLE tA = CreateThread(nullptr, 0, SendPulseThread, &paramA, 0, nullptr);
    HANDLE tB = CreateThread(nullptr, 0, SendPulseThread, &paramB, 0, nullptr);

    Sleep(50);          // make sure both threads are waiting at gate
    SetEvent(startGate); // open gate — both fire together

    AudioBuffer micBuf = g_mic.Capture(2500);

    WaitForSingleObject(tA, INFINITE);
    WaitForSingleObject(tB, INFINITE);
    CloseHandle(tA);
    CloseHandle(tB);
    CloseHandle(startGate);

    auto resultNormal = g_detector.DetectFirstArrival(micBuf, 17000.0f, 0.00020f);
    auto resultBT     = g_detector.DetectFirstArrival(micBuf, 18000.0f, 0.00007f);

    LogRecv(17000.0f, resultNormal);
    LogRecv(18000.0f, resultBT);

    if (resultNormal && resultBT) {
        int64_t offsetMs = (int64_t)resultBT->timestampMs - (int64_t)resultNormal->timestampMs;
        std::cout << "\n  [OFFSET] BT vs Normal: " << offsetMs << "ms"
                  << (offsetMs > 0 ? " (BT slower)" : " (BT faster?)") << "\n";
    }

    bool pass = resultNormal.has_value() && resultBT.has_value();
    std::cout << "\n[TEST 1] " << (pass ? "PASS" : "FAIL") << "\n";
    return pass;
}

// ── TEST 2: music + pulse, sequential ────────────────────────────────────────
bool Test2_MusicAndPulse() {
    std::cout << "\n[TEST 2] Pulse over music\n";
    std::cout << "----------------------------------------\n";

    AudioBuffer music;
    try {
        music = AudioFile::LoadWavFile("test/sound/test1.wav");
        std::cout << "[OK] Loaded test1.wav: " << music.frameCount
                  << " frames, " << music.sampleRate << "Hz\n";
    }
    catch (const std::exception& e) {
        std::cout << "[FAIL] Could not load wav: " << e.what() << "\n";
        return false;
    }

    // ── normal speaker: music + 17kHz pulse ──
    std::cout << "\n  → Normal speaker (music + pulse)\n";
    {
        SendMusicParams  mp{ &g_normal, &music };
        SendPulseParams  pp{ &g_normal, 17000.0f, 400 };  // pulse fires 400ms after music starts

        HANDLE tMusic = CreateThread(nullptr, 0, SendMusicThread, &mp, 0, nullptr);
        Sleep(300);  // let music actually start
        HANDLE tPulse = CreateThread(nullptr, 0, SendPulseThread, &pp, 0, nullptr);

        AudioBuffer micBuf = g_mic.Capture(2000);

        WaitForSingleObject(tPulse, INFINITE);
        CloseHandle(tPulse);
        // don't wait for music — let it finish in background
        CloseHandle(tMusic);

        auto result = g_detector.DetectFirstArrival(micBuf, 17000.0f, 0.00025f);
        LogRecv(17000.0f, result);
    }

    Sleep(1000);  // gap between speakers

    // ── BT speaker: music + 18kHz pulse ──
    std::cout << "\n  → BT speaker (music + pulse)\n";
    {
        SendMusicParams  mp{ &g_bt, &music };
        SendPulseParams  pp{ &g_bt, 18000.0f, 400 };

        HANDLE tMusic = CreateThread(nullptr, 0, SendMusicThread, &mp, 0, nullptr);
        Sleep(300);
        HANDLE tPulse = CreateThread(nullptr, 0, SendPulseThread, &pp, 0, nullptr);

        AudioBuffer micBuf = g_mic.Capture(2500);  // BT needs more

        WaitForSingleObject(tPulse, INFINITE);
        CloseHandle(tPulse);
        CloseHandle(tMusic);

        auto result = g_detector.DetectFirstArrival(micBuf, 18000.0f, 0.00045f);
        LogRecv(18000.0f, result);
    }

    std::cout << "\n[TEST 2] done\n";
    return true;
}

// ── runner ────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "========================================\n";
    std::cout << "  bluetooth_sync — pulse tests\n";
    std::cout << "========================================\n";

    if (!Setup()) return 1;

    int passed = 0;
    if (Test1_PulseOnly())    ++passed;
    // if (Test2_MusicAndPulse()) ++passed;

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << passed << "/2 passed\n";
    std::cout << "========================================\n";

    if (g_normal.device) g_normal.device->Release();
    if (g_bt.device)     g_bt.device->Release();

    return passed == 2 ? 0 : 1;
}