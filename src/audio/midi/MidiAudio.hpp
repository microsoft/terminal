/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MidiAudio.hpp

Abstract:
  This modules provide basic MIDI support with blocking sound output.
  */

#pragma once

#include <future>
#include <mutex>

class MidiAudio
{
public:
    MidiAudio() = default;
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
    std::promise<void> _shutdownPromise;
    std::future<void> _shutdownFuture;
    std::mutex _inUseMutex;
};
