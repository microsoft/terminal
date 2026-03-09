// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"

#include <fstream>
#include <chrono>
#include <mutex>
#include <string>

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

        // Start recording output from the given connection to the specified file path.
        // Subscribes to TerminalOutput event on the connection.
        void StartRecording(const ITerminalConnection& connection, const std::wstring& filePath, uint32_t width, uint32_t height);

        // Stop recording and close the file.
        void StopRecording();

        // Returns whether recording is currently active.
        bool IsRecording() const noexcept;

    private:
        void _WriteHeader(uint32_t width, uint32_t height);
        void _WriteEvent(const std::wstring_view& data);
        static std::string _EscapeJsonString(const std::wstring_view& input);

        std::ofstream _file;
        std::chrono::high_resolution_clock::time_point _startTime;
        bool _isRecording{ false };
        winrt::event_token _outputToken{};
        ITerminalConnection _connection{ nullptr };
        mutable std::mutex _mutex;

#ifdef UNIT_TESTING
        friend class TerminalConnectionUnitTests::AsciicastTests;
#endif
    };
}
