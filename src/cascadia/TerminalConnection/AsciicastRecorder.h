// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <fstream>
#include <chrono>
#include <atomic>
#include <thread>
#include <string>
#include <til/spsc.h>
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
    struct AsciicastMetadata
    {
        std::wstring termType; // e.g. "xterm-256color"
        std::wstring title; // tab title
        std::wstring command; // shell command
        // Theme colors in CSS #rrggbb format
        std::wstring themeFg;
        std::wstring themeBg;
        std::wstring themePalette; // colon-separated
    };

    // A single record queued from the producer (TerminalOutput callback) to
    // the writer thread via the lock-free til::spsc channel.
    struct AsciicastRecord
    {
        enum class Type : char
        {
            Output = 'o',
            Resize = 'r',
            Marker = 'm',
            Exit = 'x'
        };

        Type type{ Type::Output };
        double interval{ 0.0 }; // pre-computed interval with error diffusion applied
        std::string data; // pre-escaped UTF-8 data ready to write

        AsciicastRecord() = default;
        AsciicastRecord(Type t, double i, std::string d) :
            type{ t }, interval{ i }, data{ std::move(d) } {}
    };

    class AsciicastRecorder
    {
    public:
        AsciicastRecorder() = default;

        void StartRecording(const ITerminalConnection& connection, const std::wstring& filePath, uint32_t width, uint32_t height, const AsciicastMetadata& metadata = {});
        void StopRecording();
        void WriteInitialSnapshot(const std::wstring_view& data);
        void WriteResizeEvent(uint32_t cols, uint32_t rows);
        void WriteMarkerEvent(const std::wstring_view& label = L"");
        bool IsRecording() const noexcept;

    private:
        void _WriteHeader(uint32_t width, uint32_t height, const AsciicastMetadata& metadata);
        void _WriteEvent(const std::wstring_view& data);
        double _ComputeInterval();
        void _WriterThread(til::spsc::consumer<AsciicastRecord> rx);
        static std::string _EscapeJsonString(const std::wstring_view& input);

        // Writer thread and SPSC channel
        std::thread _writerThread;
        til::spsc::producer<AsciicastRecord> _channel{ nullptr };
        std::ofstream _file; // only accessed from the writer thread (after header)

        // Producer-side timing state (single-threaded: only the TerminalOutput
        // callback touches these, so no synchronization is needed)
        std::chrono::high_resolution_clock::time_point _lastEventTime;
        double _timingError{ 0.0 };

        // Shared atomic state
        std::atomic<bool> _isRecording{ false };
        std::atomic<bool> _producerBusy{ false }; // guards in-flight emplace calls
        std::atomic<bool> _writeError{ false }; // writer thread signals I/O errors

        // Connection management
        winrt::event_token _outputToken{};
        ITerminalConnection _connection{ nullptr };

#ifdef UNIT_TESTING
        friend class TerminalConnectionUnitTests::AsciicastTests;
#endif
    };
}
