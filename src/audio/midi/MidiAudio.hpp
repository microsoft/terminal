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

struct IDirectSound8;
struct IDirectSoundBuffer;

class MidiAudio
{
public:
    void BeginSkip() noexcept;
    void EndSkip() noexcept;
    void PlayNote(HWND windowHandle, const int noteNumber, const int velocity, const std::chrono::milliseconds duration) noexcept;

private:
    void _initialize(HWND windowHandle) noexcept;
    void _createBuffers() noexcept;

    wil::slim_event_manual_reset _skip;

    HWND _hwnd = nullptr;
    wil::unique_hmodule _directSoundModule;
    wil::com_ptr<IDirectSound8> _directSound;
    std::array<wil::com_ptr<IDirectSoundBuffer>, 2> _buffers;
    size_t _activeBufferIndex = 0;
    DWORD _lastBufferPosition = 0;
};
