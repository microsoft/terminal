// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AsciicastConnection.g.h"
#include "BaseTerminalConnection.h"

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalConnectionUnitTests
{
    class AsciicastTests;
};
#endif

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AsciicastConnection : AsciicastConnectionT<AsciicastConnection>, BaseTerminalConnection<AsciicastConnection>
    {
        AsciicastConnection() noexcept;

        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> buffer);
        void Resize(uint32_t rows, uint32_t columns) noexcept;
        void Close() noexcept;

        uint32_t InitialCols() const noexcept { return _initialCols; }
        uint32_t InitialRows() const noexcept { return _initialRows; }

        double TotalDuration() const noexcept { return _totalDuration; }
        double CurrentPosition() const noexcept { return _currentPosition.load(); }
        double PlaybackSpeed() const noexcept { return _playbackSpeed.load(); }
        void PlaybackSpeed(double value) noexcept { _playbackSpeed.store(value); }

        void PausePlayback();
        void ResumePlayback();
        void SeekPlayback(double position);
        void RestartPlayback();

        bool IsPaused() const noexcept { return _paused.load(); }

        til::event<TerminalOutputHandler> TerminalOutput;
        til::typed_event<TerminalConnection::AsciicastConnection, Windows::Foundation::IInspectable> PlaybackPositionChanged;
        til::typed_event<TerminalConnection::AsciicastConnection, Windows::Foundation::IInspectable> PlaybackStateChanged;

    private:
        struct AsciicastEvent
        {
            double timestamp;
            double cumulativeTime{ 0.0 };
            std::wstring type;
            std::wstring data;
        };

        static std::wstring _unescapeJsonString(std::wstring_view input);
        void _parseFile();
        winrt::fire_and_forget _replayEvents();
        void _emitAllOutputUpTo(size_t endIndex);

        hstring _filePath;
        std::vector<AsciicastEvent> _events;
        std::atomic<bool> _cancelled{ false };
        std::atomic<bool> _playbackFinished{ false };
        std::atomic<bool> _paused{ false };
        std::atomic<double> _playbackSpeed{ 1.0 };
        std::atomic<double> _currentPosition{ 0.0 };
        std::atomic<size_t> _seekTarget{ SIZE_MAX };
        double _totalDuration{ 0.0 };
        std::atomic<size_t> _currentEventIndex{ 0 };
        int _formatVersion{ 2 };
        double _idleTimeLimit{ 300.0 };
        uint32_t _initialCols{ 0 };
        uint32_t _initialRows{ 0 };
        bool _autoResize{ false };

#ifdef UNIT_TESTING
        friend class TerminalConnectionUnitTests::AsciicastTests;
#endif
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(AsciicastConnection);
}
