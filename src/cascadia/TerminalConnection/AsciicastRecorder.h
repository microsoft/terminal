// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <fstream>
#include <chrono>
#include <mutex>
#include <string>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalConnectionUnitTests
{
    class AsciicastTests;
};
#endif

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    class AsciicastRecorder
    {
    public:
        AsciicastRecorder() = default;

        void StartRecording(const ITerminalConnection& connection, const std::wstring& filePath, uint32_t width, uint32_t height);
        void StopRecording();
        void WriteInitialSnapshot(const std::wstring_view& data);
        bool IsRecording() const noexcept;

    private:
        void _WriteHeader(uint32_t width, uint32_t height);
        void _WriteEvent(const std::wstring_view& data);
        void _WriteExitEvent();
        static std::string _EscapeJsonString(const std::wstring_view& input);

        std::ofstream _file;
        std::chrono::high_resolution_clock::time_point _lastEventTime;
        bool _isRecording{ false };
        winrt::event_token _outputToken{};
        ITerminalConnection _connection{ nullptr };
        mutable std::mutex _mutex;

#ifdef UNIT_TESTING
        friend class TerminalConnectionUnitTests::AsciicastTests;
#endif
    };
}
