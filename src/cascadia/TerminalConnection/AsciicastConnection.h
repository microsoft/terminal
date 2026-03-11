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

        til::event<TerminalOutputHandler> TerminalOutput;

    private:
        struct AsciicastEvent
        {
            double timestamp;
            std::wstring type;
            std::wstring data;
        };

        static std::wstring _unescapeJsonString(std::wstring_view input);
        void _parseFile();
        winrt::fire_and_forget _replayEvents();

        hstring _filePath;
        std::vector<AsciicastEvent> _events;
        std::atomic<bool> _cancelled{ false };
        std::atomic<bool> _playbackFinished{ false };
        int _formatVersion{ 2 };

#ifdef UNIT_TESTING
        friend class TerminalConnectionUnitTests::AsciicastTests;
#endif
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(AsciicastConnection);
}
