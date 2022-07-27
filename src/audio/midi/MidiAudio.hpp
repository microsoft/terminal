/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MidiAudio.hpp

Abstract:
  This modules provide basic MIDI support with blocking sound output.
  */

#pragma once

#include <array>
#include <future>
#include <mutex>

struct IDirectSound8;
struct IDirectSoundBuffer;

class MidiAudio
{
public:
    MidiAudio(HWND windowHandle);
    MidiAudio(const MidiAudio&) = delete;
    MidiAudio(MidiAudio&&) = delete;
    MidiAudio& operator=(const MidiAudio&) = delete;
    MidiAudio& operator=(MidiAudio&&) = delete;
    ~MidiAudio() noexcept;
    void Initialize();
    void Shutdown();
    void Lock();
    void Unlock();
    void PlayNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration) noexcept;

private:
    void _createBuffers() noexcept;

    Microsoft::WRL::ComPtr<IDirectSound8> _directSound;
    std::array<Microsoft::WRL::ComPtr<IDirectSoundBuffer>, 2> _buffers;
    size_t _activeBufferIndex = 0;
    DWORD _lastBufferPosition = 0;
    std::promise<void> _shutdownPromise;
    std::future<void> _shutdownFuture;
    std::mutex _inUseMutex;
};
