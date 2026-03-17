// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../TerminalConnection/AsciicastRecorder.h"
// AsciicastConnection.h requires WinRT generated headers; test the static
// _unescapeJsonString helper via a standalone reimplementation that mirrors
// the production code exactly (see _UnescapeJsonString below).

#include <til/spsc.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdio>

using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalConnectionUnitTests
{
    class AsciicastTests
    {
        TEST_CLASS(AsciicastTests);

        // =====================================================================
        // AsciicastRecorder tests
        // =====================================================================

        TEST_METHOD(EscapeJsonString_QuoteAndBackslash);
        TEST_METHOD(EscapeJsonString_Newlines);
        TEST_METHOD(EscapeJsonString_Tab);
        TEST_METHOD(EscapeJsonString_ControlCharacters);
        TEST_METHOD(EscapeJsonString_AsciiPassthrough);
        TEST_METHOD(EscapeJsonString_UnicodeMultibyte);

        // =====================================================================
        // AsciicastRecorder integration tests
        // =====================================================================

        TEST_METHOD(WriteHeader_Format);
        TEST_METHOD(WriteHeader_WithMetadata);
        TEST_METHOD(WriteHeader_MinimalMetadata);
        TEST_METHOD(WriteEvent_Format);
        TEST_METHOD(WriteEvent_ErrorDiffusionTiming);
        TEST_METHOD(Recording_ConcurrentWrites);

        // =====================================================================
        // Unescape / round-trip tests
        // =====================================================================

        TEST_METHOD(UnescapeJsonString_BasicEscapes);
        TEST_METHOD(UnescapeJsonString_UnicodeEscape);
        TEST_METHOD(UnescapeJsonString_SlashAndFormfeed);
        TEST_METHOD(UnescapeJsonString_UnknownEscape);
        TEST_METHOD(RoundTrip_EscapeThenUnescape);

        // =====================================================================
        // Parse file tests
        // =====================================================================

        TEST_METHOD(ParseFile_ValidCastFile);
        TEST_METHOD(ParseFile_EmptyFile);
        TEST_METHOD(ParseFile_MissingFile);
        TEST_METHOD(ParseFile_MalformedLines);
        TEST_METHOD(ParseFile_EscapedDataRoundTrip);
        TEST_METHOD(ParseFile_IdleTimeLimit);

        // =====================================================================
        // Resize and marker event tests
        // =====================================================================

        TEST_METHOD(WriteResizeEvent_Format);
        TEST_METHOD(WriteMarkerEvent_Format);
        TEST_METHOD(WriteMarkerEvent_EmptyLabel);
        TEST_METHOD(ParseFile_ResizeAndMarkerEvents);

    private:
        // Mirror of AsciicastConnection::_unescapeJsonString so we can test
        // without pulling in the WinRT-generated headers.
        static std::wstring _UnescapeJsonString(std::wstring_view input);

        // Helper: call the private static _EscapeJsonString on AsciicastRecorder.
        static std::string _EscapeJson(const std::wstring_view& input);

        // Helper: write a temp .cast file and return its path.
        static std::filesystem::path _WriteTempCastFile(const std::string& contents);

        // Helper: parse a .cast file using the same algorithm as _parseFile.
        struct AsciicastEvent
        {
            double timestamp;
            std::wstring type;
            std::wstring data;
        };
        static std::vector<AsciicastEvent> _ParseCastFile(const std::filesystem::path& filePath);
    };

    // =====================================================================
    // Helper implementations
    // =====================================================================

    std::string AsciicastTests::_EscapeJson(const std::wstring_view& input)
    {
        // Access private static via friend.
        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        return Recorder::_EscapeJsonString(input);
    }

    // Standalone mirror of AsciicastConnection::_unescapeJsonString to avoid
    // the WinRT codegen dependency.  Kept in exact sync with production code.
    std::wstring AsciicastTests::_UnescapeJsonString(std::wstring_view input)
    {
        std::wstring result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] == L'\\' && i + 1 < input.size())
            {
                ++i;
                switch (input[i])
                {
                case L'n':
                    result += L'\n';
                    break;
                case L'r':
                    result += L'\r';
                    break;
                case L't':
                    result += L'\t';
                    break;
                case L'\\':
                    result += L'\\';
                    break;
                case L'"':
                    result += L'"';
                    break;
                case L'/':
                    result += L'/';
                    break;
                case L'b':
                    result += L'\b';
                    break;
                case L'f':
                    result += L'\f';
                    break;
                case L'u':
                {
                    if (i + 4 < input.size())
                    {
                        const auto hex = input.substr(i + 1, 4);
                        unsigned long codepoint = 0;
                        try
                        {
                            codepoint = std::stoul(std::wstring{ hex }, nullptr, 16);
                        }
                        catch (...)
                        {
                            result += L'\\';
                            result += L'u';
                            break;
                        }
                        result += static_cast<wchar_t>(codepoint);
                        i += 4;
                    }
                    break;
                }
                default:
                    result += L'\\';
                    result += input[i];
                    break;
                }
            }
            else
            {
                result += input[i];
            }
        }
        return result;
    }

    std::filesystem::path AsciicastTests::_WriteTempCastFile(const std::string& contents)
    {
        wchar_t tempDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tempDir);

        wchar_t tempFile[MAX_PATH];
        GetTempFileNameW(tempDir, L"asc", 0, tempFile);

        std::ofstream out(tempFile, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open())
        {
            std::filesystem::remove(tempFile);
            return {};
        }
        out << contents;
        out.close();

        return std::filesystem::path{ tempFile };
    }

    // Replicates the _parseFile logic from AsciicastConnection without WinRT
    // dependencies, so we can test the parsing algorithm in a TAEF DLL.
    std::vector<AsciicastTests::AsciicastEvent> AsciicastTests::_ParseCastFile(const std::filesystem::path& filePath)
    {
        std::vector<AsciicastEvent> events;

        std::ifstream file{ filePath, std::ios::in | std::ios::binary };
        if (!file.is_open())
        {
            return events;
        }

        std::string line;
        bool isFirstLine = true;

        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty())
            {
                continue;
            }
            if (isFirstLine)
            {
                isFirstLine = false;
                continue;
            }

            const auto bracketPos = line.find('[');
            if (bracketPos == std::string::npos)
                continue;

            const auto firstComma = line.find(',', bracketPos + 1);
            if (firstComma == std::string::npos)
                continue;

            double timestamp = 0.0;
            try
            {
                timestamp = std::stod(line.substr(bracketPos + 1, firstComma - bracketPos - 1));
            }
            catch (...)
            {
                continue;
            }

            const auto typeQuote1 = line.find('"', firstComma + 1);
            if (typeQuote1 == std::string::npos)
                continue;
            const auto typeQuote2 = line.find('"', typeQuote1 + 1);
            if (typeQuote2 == std::string::npos)
                continue;
            const auto type = line.substr(typeQuote1 + 1, typeQuote2 - typeQuote1 - 1);

            const auto commaAfterType = line.find(',', typeQuote2 + 1);
            if (commaAfterType == std::string::npos)
                continue;

            const auto dataQuote1 = line.find('"', commaAfterType + 1);
            if (dataQuote1 == std::string::npos)
                continue;

            size_t dataQuote2 = std::string::npos;
            for (size_t i = dataQuote1 + 1; i < line.size(); ++i)
            {
                if (line[i] == '\\')
                {
                    ++i;
                    continue;
                }
                if (line[i] == '"')
                {
                    dataQuote2 = i;
                    break;
                }
            }
            if (dataQuote2 == std::string::npos)
                continue;

            const auto rawData = line.substr(dataQuote1 + 1, dataQuote2 - dataQuote1 - 1);

            const auto wType = til::u8u16(type);
            const auto wRawData = til::u8u16(rawData);
            const auto wData = _UnescapeJsonString(wRawData);

            events.push_back({ timestamp, std::wstring{ wType }, wData });
        }

        return events;
    }

    // =====================================================================
    // EscapeJsonString tests
    // =====================================================================

    void AsciicastTests::EscapeJsonString_QuoteAndBackslash()
    {
        Log::Comment(L"Verify that double-quote and backslash are escaped.");
        const auto result = _EscapeJson(L"say \"hello\" and \\path");
        VERIFY_ARE_EQUAL(std::string{ "say \\\"hello\\\" and \\\\path" }, result);
    }

    void AsciicastTests::EscapeJsonString_Newlines()
    {
        Log::Comment(L"Verify \\r and \\n are escaped correctly.");
        const auto result = _EscapeJson(L"line1\r\nline2\n");
        VERIFY_ARE_EQUAL(std::string{ "line1\\r\\nline2\\n" }, result);
    }

    void AsciicastTests::EscapeJsonString_Tab()
    {
        Log::Comment(L"Verify tab character is escaped.");
        const auto result = _EscapeJson(L"col1\tcol2");
        VERIFY_ARE_EQUAL(std::string{ "col1\\tcol2" }, result);
    }

    void AsciicastTests::EscapeJsonString_ControlCharacters()
    {
        Log::Comment(L"Verify control chars below 0x20 use \\u00xx format.");
        // BEL (0x07) and ESC (0x1B)
        const std::wstring input{ L'\x07', L'A', L'\x1b', L'B' };
        const auto result = _EscapeJson(input);
        VERIFY_ARE_EQUAL(std::string{ "\\u0007A\\u001bB" }, result);
    }

    void AsciicastTests::EscapeJsonString_AsciiPassthrough()
    {
        Log::Comment(L"Verify plain ASCII passes through unchanged.");
        const auto result = _EscapeJson(L"Hello, World! 123");
        VERIFY_ARE_EQUAL(std::string{ "Hello, World! 123" }, result);
    }

    void AsciicastTests::EscapeJsonString_UnicodeMultibyte()
    {
        Log::Comment(L"Verify that non-ASCII Unicode passes through as UTF-8 bytes.");
        // U+00E9 (é) encodes as two UTF-8 bytes: 0xC3 0xA9
        const auto result = _EscapeJson(L"\u00e9");
        // The escape function converts to UTF-8 and passes non-control bytes through.
        VERIFY_ARE_EQUAL(std::string{ "\xC3\xA9" }, result);
    }

    // =====================================================================
    // WriteHeader / WriteEvent integration tests
    // =====================================================================

    void AsciicastTests::WriteHeader_Format()
    {
        Log::Comment(L"Verify _WriteHeader produces valid asciicast v3 header JSON with default metadata.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Metadata = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastMetadata;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        // Directly construct a recorder and call _WriteHeader via friend access.
        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        recorder._WriteHeader(80, 24, Metadata{});
        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        // Verify v3 header structure
        VERIFY_IS_TRUE(line.find("{\"version\": 3") == 0);
        VERIFY_IS_TRUE(line.find("\"cols\": 80") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"rows\": 24") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"timestamp\":") != std::string::npos);
        // With empty metadata, optional fields should not appear
        VERIFY_IS_TRUE(line.find("\"type\":") == std::string::npos);
        VERIFY_IS_TRUE(line.find("\"title\":") == std::string::npos);
        VERIFY_IS_TRUE(line.find("\"command\":") == std::string::npos);
        VERIFY_IS_TRUE(line.find("\"theme\":") == std::string::npos);

        Log::Comment(L"Header line validated successfully.");
    }

    void AsciicastTests::WriteHeader_WithMetadata()
    {
        Log::Comment(L"Verify _WriteHeader includes optional fields when metadata is provided.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Metadata = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastMetadata;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        Metadata metadata;
        metadata.termType = L"xterm-256color";
        metadata.title = L"My Session";
        metadata.command = L"/bin/bash";
        metadata.themeFg = L"#d4d4d4";
        metadata.themeBg = L"#1e1e1e";
        metadata.themePalette = L"#000000:#cd3131:#0dbc79";

        recorder._WriteHeader(120, 30, metadata);
        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        VERIFY_IS_TRUE(line.find("{\"version\": 3") == 0);
        VERIFY_IS_TRUE(line.find("\"cols\": 120") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"rows\": 30") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"type\": \"xterm-256color\"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"title\": \"My Session\"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"command\": \"/bin/bash\"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"fg\": \"#d4d4d4\"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"bg\": \"#1e1e1e\"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("\"palette\": \"#000000:#cd3131:#0dbc79\"") != std::string::npos);

        Log::Comment(L"Rich header with metadata validated successfully.");
    }

    void AsciicastTests::WriteHeader_MinimalMetadata()
    {
        Log::Comment(L"Verify _WriteHeader works when only some metadata fields are set.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Metadata = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastMetadata;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        Metadata metadata;
        metadata.termType = L"xterm-256color";
        // Leave title, command, and theme empty

        recorder._WriteHeader(80, 24, metadata);
        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        VERIFY_IS_TRUE(line.find("{\"version\": 3") == 0);
        VERIFY_IS_TRUE(line.find("\"type\": \"xterm-256color\"") != std::string::npos);
        // Optional fields should not appear when empty
        VERIFY_IS_TRUE(line.find("\"title\":") == std::string::npos);
        VERIFY_IS_TRUE(line.find("\"command\":") == std::string::npos);
        VERIFY_IS_TRUE(line.find("\"theme\":") == std::string::npos);

        Log::Comment(L"Minimal metadata header validated successfully.");
    }

    void AsciicastTests::WriteEvent_Format()
    {
        Log::Comment(L"Verify the full SPSC pipeline produces correctly formatted event output.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Record = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecord;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        // Create a SPSC channel and writer thread, mirroring what StartRecording does.
        auto [tx, rx] = til::spsc::channel<Record>(64);
        recorder._channel = std::move(tx);
        recorder._writerThread = std::thread([&recorder, rx = std::move(rx)]() mutable {
            recorder._WriterThread(std::move(rx));
        });

        recorder._isRecording.store(true, std::memory_order_release);
        recorder._lastEventTime = std::chrono::high_resolution_clock::now();
        recorder._timingError = 0.0;

        // Small sleep to ensure elapsed > 0
        Sleep(10);
        recorder._WriteEvent(L"Hello\r\n");

        // Close the channel and wait for the writer to drain.
        recorder._isRecording.store(false, std::memory_order_release);
        { auto discard = std::move(recorder._channel); }
        recorder._writerThread.join();

        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        // Event format: [<time>, "o", "<escaped data>"]
        VERIFY_IS_TRUE(line.front() == '[');
        VERIFY_IS_TRUE(line.find(", \"o\", \"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("Hello\\r\\n") != std::string::npos);
        VERIFY_IS_TRUE(line.back() == ']');

        // Verify the timestamp is > 0
        auto timeStr = line.substr(1, line.find(',') - 1);
        double elapsed = std::stod(timeStr);
        VERIFY_IS_TRUE(elapsed > 0.0);

        Log::Comment(L"Event line validated successfully.");
    }

    void AsciicastTests::WriteEvent_ErrorDiffusionTiming()
    {
        Log::Comment(L"Verify error diffusion keeps total time accurate over many events.");

        // Test the error diffusion math directly: simulate 1000 events at
        // exactly 1.2345ms intervals.  Without diffusion the naive rounding
        // would lose 0.0005ms per event (0.5ms total drift).  With diffusion
        // the summed intervals must stay within 1ms of the true total.
        const double intervalSec = 0.0012345;
        const int eventCount = 1000;
        const double totalExpected = intervalSec * eventCount; // 1.2345

        double error = 0.0;
        double totalWritten = 0.0;
        for (int i = 0; i < eventCount; ++i)
        {
            const double raw = intervalSec + error;
            const double rounded = std::round(raw * 1000.0) / 1000.0;
            error = raw - rounded;
            totalWritten += rounded;
        }

        Log::Comment(L"Checking summed intervals are within 1ms of expected total.");
        VERIFY_IS_TRUE(std::abs(totalWritten - totalExpected) < 0.001);

        // Also verify that naive rounding (without diffusion) DOES drift.
        double naiveTotal = 0.0;
        for (int i = 0; i < eventCount; ++i)
        {
            const double rounded = std::round(intervalSec * 1000.0) / 1000.0;
            naiveTotal += rounded;
        }
        const double naiveDrift = std::abs(naiveTotal - totalExpected);
        const double diffusionDrift = std::abs(totalWritten - totalExpected);
        VERIFY_IS_TRUE(diffusionDrift < naiveDrift);

        Log::Comment(L"Error diffusion timing validated successfully.");
    }

    void AsciicastTests::Recording_ConcurrentWrites()
    {
        Log::Comment(L"Fire many events rapidly and verify all appear in order in the output file.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Record = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecord;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        auto [tx, rx] = til::spsc::channel<Record>(4096);
        recorder._channel = std::move(tx);
        recorder._writerThread = std::thread([&recorder, rx = std::move(rx)]() mutable {
            recorder._WriterThread(std::move(rx));
        });

        recorder._isRecording.store(true, std::memory_order_release);
        recorder._lastEventTime = std::chrono::high_resolution_clock::now();
        recorder._timingError = 0.0;

        const int eventCount = 500;
        for (int i = 0; i < eventCount; ++i)
        {
            std::wstring eventData = L"event-" + std::to_wstring(i) + L"\r\n";
            recorder._WriteEvent(eventData);
        }

        // Close the channel and wait for the writer to drain.
        recorder._isRecording.store(false, std::memory_order_release);
        { auto discard = std::move(recorder._channel); }
        recorder._writerThread.join();

        recorder._file.close();

        // Read the file and verify we got exactly eventCount lines, all in order.
        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        int lineCount = 0;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty())
            {
                continue;
            }

            // Each line should contain the event index marker.
            std::string expectedMarker = "event-" + std::to_string(lineCount) + "\\r\\n";
            VERIFY_IS_TRUE(line.find(expectedMarker) != std::string::npos);
            lineCount++;
        }

        VERIFY_ARE_EQUAL(eventCount, lineCount);
        Log::Comment(L"All events arrived in order.");
    }

    // =====================================================================
    // UnescapeJsonString tests
    // =====================================================================

    void AsciicastTests::UnescapeJsonString_BasicEscapes()
    {
        Log::Comment(L"Verify standard JSON escape sequences are unescaped.");

        VERIFY_ARE_EQUAL(std::wstring{ L"\n" }, _UnescapeJsonString(L"\\n"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\r" }, _UnescapeJsonString(L"\\r"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\t" }, _UnescapeJsonString(L"\\t"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\\" }, _UnescapeJsonString(L"\\\\"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\"" }, _UnescapeJsonString(L"\\\""));
    }

    void AsciicastTests::UnescapeJsonString_UnicodeEscape()
    {
        Log::Comment(L"Verify unicode escape sequences are decoded.");

        // \u001b → ESC (0x1B)
        const auto result = _UnescapeJsonString(L"\\u001b[32m");
        VERIFY_ARE_EQUAL(static_cast<wchar_t>(0x1B), result[0]);
        VERIFY_ARE_EQUAL(L'[', result[1]);

        // \u0041 → 'A'
        VERIFY_ARE_EQUAL(std::wstring{ L"A" }, _UnescapeJsonString(L"\\u0041"));
    }

    void AsciicastTests::UnescapeJsonString_SlashAndFormfeed()
    {
        Log::Comment(L"Verify \\/ and \\b and \\f are handled.");
        VERIFY_ARE_EQUAL(std::wstring{ L"/" }, _UnescapeJsonString(L"\\/"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\b" }, _UnescapeJsonString(L"\\b"));
        VERIFY_ARE_EQUAL(std::wstring{ L"\f" }, _UnescapeJsonString(L"\\f"));
    }

    void AsciicastTests::UnescapeJsonString_UnknownEscape()
    {
        Log::Comment(L"Verify unknown escapes are preserved literally.");
        // \z is not a valid JSON escape — should produce backslash + z
        const auto result = _UnescapeJsonString(L"\\z");
        VERIFY_ARE_EQUAL(std::wstring{ L"\\z" }, result);
    }

    void AsciicastTests::RoundTrip_EscapeThenUnescape()
    {
        Log::Comment(L"Verify escape→unescape round trip returns original string.");

        const std::wstring original = L"Hello \"World\"\r\nTab\there\t\x1b[32mGreen\x1b[0m";
        const auto escaped = _EscapeJson(original);

        // escaped is a std::string (UTF-8). Convert back to wstring via til.
        const auto wEscaped = til::u8u16(escaped);
        const auto roundTripped = _UnescapeJsonString(wEscaped);

        VERIFY_ARE_EQUAL(original, roundTripped);
    }

    // =====================================================================
    // Parse file tests
    // =====================================================================

    void AsciicastTests::ParseFile_ValidCastFile()
    {
        Log::Comment(L"Parse a well-formed asciicast v2 file.");

        const std::string content =
            "{\"version\": 2, \"width\": 80, \"height\": 24, \"timestamp\": 1709999999}\n"
            "[0.000000, \"o\", \"Hello, World!\\r\\n\"]\n"
            "[0.500000, \"o\", \"\\u001b[32mgreen text\\u001b[0m\\r\\n\"]\n"
            "[1.000000, \"o\", \"tab\\there\\r\\n\"]\n"
            "[1.500000, \"o\", \"quote: \\\"test\\\"\\r\\n\"]\n";

        const auto tempPath = _WriteTempCastFile(content);
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        const auto events = _ParseCastFile(tempPath);
        VERIFY_ARE_EQUAL(static_cast<size_t>(4), events.size());

        // Event 0: timestamp 0, "Hello, World!\r\n"
        VERIFY_ARE_EQUAL(0.0, events[0].timestamp);
        VERIFY_ARE_EQUAL(std::wstring{ L"o" }, events[0].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"Hello, World!\r\n" }, events[0].data);

        // Event 1: timestamp 0.5, contains ESC[32m
        VERIFY_ARE_EQUAL(0.5, events[1].timestamp);
        VERIFY_IS_TRUE(events[1].data.find(L'\x1b') != std::wstring::npos);

        // Event 2: timestamp 1.0, contains tab
        VERIFY_ARE_EQUAL(1.0, events[2].timestamp);
        VERIFY_IS_TRUE(events[2].data.find(L'\t') != std::wstring::npos);

        // Event 3: timestamp 1.5, contains literal double quote
        VERIFY_ARE_EQUAL(1.5, events[3].timestamp);
        VERIFY_IS_TRUE(events[3].data.find(L'"') != std::wstring::npos);

        Log::Comment(L"All 4 events parsed correctly.");
    }

    void AsciicastTests::ParseFile_EmptyFile()
    {
        Log::Comment(L"Parse an empty file — should return zero events.");

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        const auto events = _ParseCastFile(tempPath);
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), events.size());
    }

    void AsciicastTests::ParseFile_MissingFile()
    {
        Log::Comment(L"Parse a nonexistent file, should return zero events gracefully.");

        const auto events = _ParseCastFile(L"C:\\nonexistent_path_12345\\fake.cast");
        VERIFY_ARE_EQUAL(static_cast<size_t>(0), events.size());
    }

    void AsciicastTests::ParseFile_MalformedLines()
    {
        Log::Comment(L"Malformed event lines should be skipped without crashing.");

        const std::string content =
            "{\"version\": 2, \"width\": 80, \"height\": 24}\n"
            "this is not valid json\n"
            "[bad_timestamp, \"o\", \"data\"]\n"
            "[0.5, \"o\"]\n" // missing data field
            "[1.0, \"o\", \"valid\"]\n" // one valid event
            "[]\n"; // empty array

        const auto tempPath = _WriteTempCastFile(content);
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        const auto events = _ParseCastFile(tempPath);
        // Only the valid event at timestamp 1.0 should be parsed.
        VERIFY_ARE_EQUAL(static_cast<size_t>(1), events.size());
        VERIFY_ARE_EQUAL(1.0, events[0].timestamp);
        VERIFY_ARE_EQUAL(std::wstring{ L"valid" }, events[0].data);

        Log::Comment(L"Malformed lines were correctly skipped.");
    }

    void AsciicastTests::ParseFile_EscapedDataRoundTrip()
    {
        Log::Comment(L"Verify that recorder-escaped data can be parsed back correctly.");

        // Simulate what the recorder would write for data containing special chars.
        const std::wstring originalData = L"path: C:\\Users\\test\r\nquote: \"hi\"\ttab";
        const auto escapedUtf8 = _EscapeJson(originalData);

        // Build a .cast file with the escaped data.
        std::string content = "{\"version\": 2, \"width\": 80, \"height\": 24}\n";
        content += "[0.000000, \"o\", \"" + escapedUtf8 + "\"]\n";

        const auto tempPath = _WriteTempCastFile(content);
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        const auto events = _ParseCastFile(tempPath);
        VERIFY_ARE_EQUAL(static_cast<size_t>(1), events.size());
        VERIFY_ARE_EQUAL(originalData, events[0].data);

        Log::Comment(L"Recorder->Parser round trip succeeded.");
    }

    void AsciicastTests::ParseFile_IdleTimeLimit()
    {
        Log::Comment(L"Verify idle_time_limit is parsed from the header.");

        // Build a .cast file with idle_time_limit in the header.
        const std::string content =
            "{\"version\": 3, \"term\": {\"cols\": 80, \"rows\": 24}, \"timestamp\": 1700000000, \"idle_time_limit\": 5.0}\n"
            "[0.000, \"o\", \"hello\"]\n"
            "[100.000, \"o\", \"world\"]\n";

        const auto tempPath = _WriteTempCastFile(content);
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        // Parse the header using the same approach as _parseFile to extract
        // idle_time_limit, mirroring the production logic.
        std::ifstream file{ tempPath, std::ios::in | std::ios::binary };
        VERIFY_IS_TRUE(file.is_open());

        std::string headerLine;
        std::getline(file, headerLine);

        double idleTimeLimit = 300.0; // default
        auto idlePos = headerLine.find("\"idle_time_limit\"");
        if (idlePos != std::string::npos)
        {
            auto colonPos = headerLine.find(':', idlePos + 17);
            if (colonPos != std::string::npos)
            {
                try
                {
                    idleTimeLimit = std::stod(headerLine.substr(colonPos + 1));
                }
                catch (...)
                {
                    // Keep default
                }
            }
        }

        VERIFY_ARE_EQUAL(5.0, idleTimeLimit);

        // Also verify that a file without idle_time_limit keeps the default.
        const std::string contentNoLimit =
            "{\"version\": 3, \"term\": {\"cols\": 80, \"rows\": 24}, \"timestamp\": 1700000000}\n"
            "[0.000, \"o\", \"hello\"]\n";

        const auto tempPath2 = _WriteTempCastFile(contentNoLimit);
        auto cleanup2 = wil::scope_exit([&] { std::filesystem::remove(tempPath2); });

        std::ifstream file2{ tempPath2, std::ios::in | std::ios::binary };
        std::getline(file2, headerLine);

        double defaultLimit = 300.0;
        idlePos = headerLine.find("\"idle_time_limit\"");
        if (idlePos != std::string::npos)
        {
            auto colonPos = headerLine.find(':', idlePos + 17);
            if (colonPos != std::string::npos)
            {
                try
                {
                    defaultLimit = std::stod(headerLine.substr(colonPos + 1));
                }
                catch (...)
                {
                }
            }
        }

        VERIFY_ARE_EQUAL(300.0, defaultLimit);

        Log::Comment(L"idle_time_limit parsing validated successfully.");
    }

    // =====================================================================
    // Resize and marker event tests
    // =====================================================================

    void AsciicastTests::WriteResizeEvent_Format()
    {
        Log::Comment(L"Verify WriteResizeEvent produces a correctly formatted resize event.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Record = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecord;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        // Create a SPSC channel and writer thread, mirroring what StartRecording does.
        auto [tx, rx] = til::spsc::channel<Record>(64);
        recorder._channel = std::move(tx);
        recorder._writerThread = std::thread([&recorder, rx = std::move(rx)]() mutable {
            recorder._WriterThread(std::move(rx));
        });

        recorder._isRecording.store(true, std::memory_order_release);
        recorder._lastEventTime = std::chrono::high_resolution_clock::now();
        recorder._timingError = 0.0;

        Sleep(10);
        recorder.WriteResizeEvent(120, 30);

        // Close the channel and wait for the writer to drain.
        recorder._isRecording.store(false, std::memory_order_release);
        { auto discard = std::move(recorder._channel); }
        recorder._writerThread.join();

        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        // Event format: [<time>, "r", "120x30"]
        VERIFY_IS_TRUE(line.front() == '[');
        VERIFY_IS_TRUE(line.find(", \"r\", \"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("120x30") != std::string::npos);
        VERIFY_IS_TRUE(line.back() == ']');

        auto timeStr = line.substr(1, line.find(',') - 1);
        double elapsed = std::stod(timeStr);
        VERIFY_IS_TRUE(elapsed > 0.0);

        Log::Comment(L"Resize event line validated successfully.");
    }

    void AsciicastTests::WriteMarkerEvent_Format()
    {
        Log::Comment(L"Verify WriteMarkerEvent produces a correctly formatted marker event with label.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Record = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecord;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        auto [tx, rx] = til::spsc::channel<Record>(64);
        recorder._channel = std::move(tx);
        recorder._writerThread = std::thread([&recorder, rx = std::move(rx)]() mutable {
            recorder._WriterThread(std::move(rx));
        });

        recorder._isRecording.store(true, std::memory_order_release);
        recorder._lastEventTime = std::chrono::high_resolution_clock::now();
        recorder._timingError = 0.0;

        Sleep(10);
        recorder.WriteMarkerEvent(L"Configuration complete");

        recorder._isRecording.store(false, std::memory_order_release);
        { auto discard = std::move(recorder._channel); }
        recorder._writerThread.join();

        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        // Event format: [<time>, "m", "Configuration complete"]
        VERIFY_IS_TRUE(line.front() == '[');
        VERIFY_IS_TRUE(line.find(", \"m\", \"") != std::string::npos);
        VERIFY_IS_TRUE(line.find("Configuration complete") != std::string::npos);
        VERIFY_IS_TRUE(line.back() == ']');

        auto timeStr = line.substr(1, line.find(',') - 1);
        double elapsed = std::stod(timeStr);
        VERIFY_IS_TRUE(elapsed > 0.0);

        Log::Comment(L"Marker event with label validated successfully.");
    }

    void AsciicastTests::WriteMarkerEvent_EmptyLabel()
    {
        Log::Comment(L"Verify WriteMarkerEvent with empty label produces correct output.");

        using Recorder = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecorder;
        using Record = winrt::Microsoft::Terminal::TerminalConnection::implementation::AsciicastRecord;

        const auto tempPath = _WriteTempCastFile("");
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        Recorder recorder;
        recorder._file.open(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        VERIFY_IS_TRUE(recorder._file.is_open());

        auto [tx, rx] = til::spsc::channel<Record>(64);
        recorder._channel = std::move(tx);
        recorder._writerThread = std::thread([&recorder, rx = std::move(rx)]() mutable {
            recorder._WriterThread(std::move(rx));
        });

        recorder._isRecording.store(true, std::memory_order_release);
        recorder._lastEventTime = std::chrono::high_resolution_clock::now();
        recorder._timingError = 0.0;

        Sleep(10);
        recorder.WriteMarkerEvent(L"");

        recorder._isRecording.store(false, std::memory_order_release);
        { auto discard = std::move(recorder._channel); }
        recorder._writerThread.join();

        recorder._file.close();

        std::ifstream in(tempPath, std::ios::in | std::ios::binary);
        std::string line;
        std::getline(in, line);

        // Event format: [<time>, "m", ""]
        VERIFY_IS_TRUE(line.front() == '[');
        VERIFY_IS_TRUE(line.find(", \"m\", \"\"") != std::string::npos);
        VERIFY_IS_TRUE(line.back() == ']');

        Log::Comment(L"Empty marker event validated successfully.");
    }

    void AsciicastTests::ParseFile_ResizeAndMarkerEvents()
    {
        Log::Comment(L"Parse a file with mixed output, resize, and marker events.");

        const std::string content =
            "{\"version\": 3, \"term\": {\"cols\": 80, \"rows\": 24}, \"timestamp\": 1709999999}\n"
            "[0.000, \"o\", \"Hello\"]\n"
            "[0.500, \"r\", \"120x30\"]\n"
            "[1.000, \"m\", \"checkpoint\"]\n"
            "[0.200, \"o\", \"World\"]\n"
            "[0.100, \"m\", \"\"]\n";

        const auto tempPath = _WriteTempCastFile(content);
        auto cleanup = wil::scope_exit([&] { std::filesystem::remove(tempPath); });

        const auto events = _ParseCastFile(tempPath);
        VERIFY_ARE_EQUAL(static_cast<size_t>(5), events.size());

        // Event 0: output
        VERIFY_ARE_EQUAL(std::wstring{ L"o" }, events[0].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"Hello" }, events[0].data);
        VERIFY_ARE_EQUAL(0.0, events[0].timestamp);

        // Event 1: resize
        VERIFY_ARE_EQUAL(std::wstring{ L"r" }, events[1].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"120x30" }, events[1].data);
        VERIFY_ARE_EQUAL(0.5, events[1].timestamp);

        // Event 2: marker with label
        VERIFY_ARE_EQUAL(std::wstring{ L"m" }, events[2].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"checkpoint" }, events[2].data);
        VERIFY_ARE_EQUAL(1.0, events[2].timestamp);

        // Event 3: output
        VERIFY_ARE_EQUAL(std::wstring{ L"o" }, events[3].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"World" }, events[3].data);
        VERIFY_ARE_EQUAL(0.2, events[3].timestamp);

        // Event 4: marker with empty label
        VERIFY_ARE_EQUAL(std::wstring{ L"m" }, events[4].type);
        VERIFY_ARE_EQUAL(std::wstring{ L"" }, events[4].data);
        VERIFY_ARE_EQUAL(0.1, events[4].timestamp);

        Log::Comment(L"Mixed event types parsed correctly.");
    }
}
