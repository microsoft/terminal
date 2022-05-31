// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "MidiAudio.hpp"
#include "../terminal/parser/stateMachine.hpp"

namespace
{
    class MidiOut
    {
    public:
        static constexpr auto NOTE_OFF = 0x80;
        static constexpr auto NOTE_ON = 0x90;
        static constexpr auto PROGRAM_CHANGE = 0xC0;

        // We're using a square wave as an approximation of the sound that the
        // original VT525 terminals might have produced. This is probably not
        // quite right, but it works reasonably well.
        static constexpr auto SQUARE_WAVE_SYNTH = 80;

        MidiOut() noexcept
        {
            midiOutOpen(&handle, MIDI_MAPPER, NULL, NULL, CALLBACK_NULL);
            OutputMessage(PROGRAM_CHANGE, SQUARE_WAVE_SYNTH);
        }
        ~MidiOut() noexcept
        {
            midiOutClose(handle);
        }
        void OutputMessage(const int b1, const int b2, const int b3 = 0, const int b4 = 0) noexcept
        {
            midiOutShortMsg(handle, MAKELONG(MAKEWORD(b1, b2), MAKEWORD(b3, b4)));
        }

        MidiOut(const MidiOut&) = delete;
        MidiOut(MidiOut&&) = delete;
        MidiOut& operator=(const MidiOut&) = delete;
        MidiOut& operator=(MidiOut&&) = delete;

    private:
        HMIDIOUT handle = nullptr;
    };
}

using namespace std::chrono_literals;

MidiAudio::~MidiAudio() noexcept
{
    try
    {
#pragma warning(suppress : 26447)
        // We acquire the lock here so the class isn't destroyed while in use.
        // If this throws, we'll catch it, so the C26447 warning is bogus.
        _inUseMutex.lock();
    }
    catch (...)
    {
        // If the lock fails, we'll just have to live with the consequences.
    }
}

void MidiAudio::Initialize()
{
    _shutdownFuture = _shutdownPromise.get_future();
}

void MidiAudio::Shutdown()
{
    // Once the shutdown promise is set, any note that is playing will stop
    // immediately, and the Unlock call will exit the thread ASAP.
    _shutdownPromise.set_value();
}

void MidiAudio::Lock()
{
    _inUseMutex.lock();
}

void MidiAudio::Unlock()
{
    // We need to check the shutdown status before releasing the mutex,
    // because after that the class could be destroyed.
    const auto shutdownStatus = _shutdownFuture.wait_for(0s);
    _inUseMutex.unlock();
    // If the wait didn't timeout, that means the shutdown promise was set,
    // so we need to exit the thread ASAP by throwing an exception.
    if (shutdownStatus != std::future_status::timeout)
    {
        throw Microsoft::Console::VirtualTerminal::StateMachine::ShutdownException{};
    }
}

void MidiAudio::PlayNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration) noexcept
try
{
    // The MidiOut is a local static because we can only have one instance,
    // and we only want to construct it when it's actually needed.
    static MidiOut midiOut;

    if (velocity)
    {
        midiOut.OutputMessage(MidiOut::NOTE_ON, noteNumber, velocity);
    }

    // By waiting on the shutdown future with the duration of the note, we'll
    // either be paused for the appropriate amount of time, or we'll break out
    // of the wait early if we've been shutdown.
    _shutdownFuture.wait_for(duration);

    if (velocity)
    {
        midiOut.OutputMessage(MidiOut::NOTE_OFF, noteNumber, velocity);
    }
}
CATCH_LOG()
