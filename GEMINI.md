# Bluetooth Speaker Sync

A Windows C++ application designed to synchronize and play audio across Bluetooth speakers using WASAPI and the MMDevice API.

## Project Architecture

The project is structured around a few core components:

### 1. Audio Device Management (`src/audio_device.h/cpp`)
- **`AudioDevice` Class**: The primary interface for hardware interaction.
  - `GetBluetoothSpeakers()`: Uses `IMMDeviceEnumerator` to find and filter audio endpoints specifically for Bluetooth devices.
  - `SendAudioToSpeaker()`: Manages the WASAPI playback loop. It initializes `IAudioClient`, sets up the buffer, and streams `AudioBuffer` data to the device.
- **`BluetoothSpeaker` Struct**: Represents a discovered device, containing its ID, friendly name, activity status, and a pointer to its `IMMDevice` interface.

### 2. Audio File Handling (`src/audio_file.h/cpp`)
- **`AudioFile` Class**: Handles the parsing of WAV files.
  - `LoadWavFile()`: Reads RIFF/WAVE headers and extracts PCM data into an `AudioBuffer`. Currently supports standard PCM WAV formats.
- **`AudioBuffer` Struct**: A generic container for raw audio data (`std::vector<float>`), sample rate, channel count, and frame count.

### 3. Logging & Utilities
- **`Logger` (`src/logger.h/cpp`)**: A thread-safe singleton logger that writes to both `log/app.log` and the console. Supports different log levels (INFO, ERROR).
- **`hresult_utils` (`src/hresult_utils.h/cpp`)**: Provides helper functions to convert Windows `HRESULT` codes into human-readable strings and handle error checking (`LogIfFailed`, `ThrowIfFailed`).

## Development Setup

### Prerequisites
- **OS**: Windows (Required for WASAPI/COM APIs).
- **Compiler**: A C++17 compliant compiler (e.g., MSVC).
- **Build System**: CMake 3.20 or higher.
- **Windows SDK**: Necessary for multimedia and COM headers.

### Build Instructions
1. Create a build directory: `mkdir build && cd build`
2. Generate build files: `cmake ..`
3. Build the project: `cmake --build .`

The executable `bluetooth_sync.exe` will be located in `build/bin/`.

## Workflow & Conventions

### Adding New Features
- **COM Management**: The project uses COM interfaces. Ensure proper initialization with `CoInitialize` (handled in `AudioDevice` constructor) and always `Release()` interfaces when finished.
- **Error Handling**: Use the utilities in `hresult_utils.h` for all Windows API calls that return `HRESULT`. Prefer `ThrowIfFailed` for fatal setup errors and `LogIfFailed` for recoverable runtime issues.
- **Logging**: Use the convenience macros:
  - `LOG_INFO("message")`
  - `LOG_ERROR("message")`

### Testing
- Place test audio files in `test/sound/`.
- The default test file is `test/sound/test1.wav`.
- Logs are generated in the `log/` directory.

## Current Limitations
- Currently only plays on the first active Bluetooth speaker found.
- Supports only WAV (PCM) format.
- Fixed to 32-bit float audio processing internally.
