// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "MidiAudio.hpp"
#include "../terminal/parser/stateMachine.hpp"

using Microsoft::WRL::ComPtr;
using namespace std::chrono_literals;

// The WAVE_DATA below is an 8-bit PCM encoding of a triangle wave form.
// We just play this on repeat at varying frequencies to produce our notes.
constexpr auto WAVE_SIZE = 16u;
constexpr auto WAVE_DATA = std::array<byte, WAVE_SIZE>{ 128, 159, 191, 223, 255, 223, 191, 159, 128, 96, 64, 32, 0, 32, 64, 96 };

void MidiAudio::_initialize(HWND windowHandle) noexcept
{
    _hwnd = windowHandle;
    _directSoundModule.reset(LoadLibraryExW(L"dsound.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (_directSoundModule)
    {
        if (const auto createFunction = GetProcAddressByFunctionDeclaration(_directSoundModule.get(), DirectSoundCreate8))
        {
            if (SUCCEEDED(createFunction(nullptr, &_directSound, nullptr)))
            {
                if (SUCCEEDED(_directSound->SetCooperativeLevel(windowHandle, DSSCL_NORMAL)))
                {
                    _createBuffers();
                }
            }
        }
    }
}

void MidiAudio::BeginSkip() noexcept
{
    _skip.SetEvent();
}

void MidiAudio::EndSkip() noexcept
{
    _skip.ResetEvent();
}

void MidiAudio::PlayNote(HWND windowHandle, const int noteNumber, const int velocity, const std::chrono::milliseconds duration) noexcept
try
{
    if (_skip.is_signaled())
    {
        return;
    }

    if (_hwnd != windowHandle)
    {
        _initialize(windowHandle);
    }

    const auto& buffer = _buffers.at(_activeBufferIndex);
    if (velocity && buffer)
    {
        // The formula for frequency is 2^(n/12) * 440Hz, where n is zero for
        // the A above middle C (A4). In MIDI terms, A4 is note number 69,
        // which is why we subtract 69. We also need to multiply by the size
        // of the wave form to determine the frequency that the sound buffer
        // has to be played to achieve the equivalent note frequency.
        const auto frequency = std::pow(2.0, (noteNumber - 69.0) / 12.0) * 440.0 * WAVE_SIZE;
        buffer->SetFrequency(gsl::narrow_cast<DWORD>(frequency));
        // For the volume, we're using the formula defined in the General
        // MIDI Level 2 specification: Gain in dB = 40 * log10(v/127). We need
        // to multiply by 4000, though, because the SetVolume method expects
        // the volume to be in hundredths of a decibel.
        const auto volume = 4000.0 * std::log10(velocity / 127.0);
        buffer->SetVolume(gsl::narrow_cast<LONG>(volume));
        // Resetting the buffer to a position that is slightly off from the
        // last position will help to produce a clearer separation between
        // tones when repeating sequences of the same note.
        buffer->SetCurrentPosition((_lastBufferPosition + 12) % WAVE_SIZE);
    }

    // By waiting on the skip event with a maximum duration of the note, we'll
    // either be paused for the appropriate amount of time, or we'll break out early
    // because BeginSkip() was called. This happens for Ctrl+C or during shutdown.
    _skip.wait(::base::saturated_cast<DWORD>(duration.count()));

    if (velocity && buffer)
    {
        // When the note ends, we just turn the volume down instead of stopping
        // the sound buffer. This helps reduce unwanted static between notes.
        buffer->SetVolume(DSBVOLUME_MIN);
        buffer->GetCurrentPosition(&_lastBufferPosition, nullptr);
    }

    // Cycling between multiple buffers can also help reduce the static.
    _activeBufferIndex = (_activeBufferIndex + 1) % _buffers.size();
}
CATCH_LOG()

void MidiAudio::_createBuffers() noexcept
{
    auto waveFormat = WAVEFORMATEX{};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = 8000;
    waveFormat.wBitsPerSample = 8;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    auto bufferDescription = DSBUFFERDESC{};
    bufferDescription.dwSize = sizeof(DSBUFFERDESC);
    bufferDescription.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS;
    bufferDescription.dwBufferBytes = WAVE_SIZE;
    bufferDescription.lpwfxFormat = &waveFormat;

    for (auto& buffer : _buffers)
    {
        if (SUCCEEDED(_directSound->CreateSoundBuffer(&bufferDescription, &buffer, nullptr)))
        {
            LPVOID bufferPtr;
            DWORD bufferSize;
            if (SUCCEEDED(buffer->Lock(0, 0, &bufferPtr, &bufferSize, nullptr, nullptr, DSBLOCK_ENTIREBUFFER)))
            {
                std::memcpy(bufferPtr, WAVE_DATA.data(), WAVE_DATA.size());
                buffer->Unlock(bufferPtr, bufferSize, nullptr, 0);
            }
            buffer->SetVolume(DSBVOLUME_MIN);
            buffer->Play(0, 0, DSBPLAY_LOOPING);
        }
    }
}
