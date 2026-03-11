// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AsciicastRecorder.h"

#include <cstdio>
#include <ctime>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    void AsciicastRecorder::StartRecording(const ITerminalConnection& connection, const std::wstring& filePath, uint32_t width, uint32_t height)
    {
        std::lock_guard<std::mutex> lock{ _mutex };

        if (_isRecording)
        {
            return;
        }

        _file.open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!_file.is_open())
        {
            return;
        }

        _connection = connection;
        _WriteHeader(width, height);
        _lastEventTime = std::chrono::high_resolution_clock::now();

        // Safe to capture `this`: ControlCore owns the recorder via unique_ptr
        // and calls StopRecording() (which revokes this token) before destruction.
        _outputToken = _connection.TerminalOutput([this](const winrt::array_view<const char16_t> data) {
            const std::wstring wstr{ data.begin(), data.end() };
            _WriteEvent(wstr);
        });

        _isRecording = true;
    }

    void AsciicastRecorder::StopRecording()
    {
        std::lock_guard<std::mutex> lock{ _mutex };

        if (!_isRecording)
        {
            return;
        }

        _connection.TerminalOutput(_outputToken);
        _outputToken = {};
        _connection = nullptr;
        _isRecording = false;
        _WriteExitEvent();
        _file.close();
    }

    void AsciicastRecorder::WriteInitialSnapshot(const std::wstring_view& data)
    {
        std::lock_guard<std::mutex> lock{ _mutex };

        if (!_isRecording || data.empty())
        {
            return;
        }

        const auto escaped = _EscapeJsonString(data);
        _file << "[0.000, \"o\", \"" << escaped << "\"]\n";
        _file.flush();
    }

    bool AsciicastRecorder::IsRecording() const noexcept
    {
        std::lock_guard<std::mutex> lock{ _mutex };
        return _isRecording;
    }

    void AsciicastRecorder::_WriteHeader(uint32_t width, uint32_t height)
    {
        const auto timestamp = std::time(nullptr);
        _file << "{\"version\": 3, \"term\": {\"cols\": " << width
              << ", \"rows\": " << height
              << "}, \"timestamp\": " << timestamp << "}\n";
        _file.flush();
    }

    void AsciicastRecorder::_WriteEvent(const std::wstring_view& data)
    {
        std::lock_guard<std::mutex> lock{ _mutex };

        if (!_isRecording)
        {
            return;
        }

        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> interval = now - _lastEventTime;
        _lastEventTime = now;
        const auto escaped = _EscapeJsonString(data);

        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%.3f", interval.count());

        _file << "[" << timeBuf << ", \"o\", \"" << escaped << "\"]\n";
        _file.flush();

        if (!_file.good())
        {
            _isRecording = false;
            return;
        }
    }

    void AsciicastRecorder::_WriteExitEvent()
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> interval = now - _lastEventTime;

        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "%.3f", interval.count());

        _file << "[" << timeBuf << ", \"x\", \"0\"]\n";
        _file.flush();
    }

    std::string AsciicastRecorder::_EscapeJsonString(const std::wstring_view& input)
    {
        const auto utf8 = til::u16u8(input);
        std::string result;
        result.reserve(utf8.size());

        for (const auto ch : utf8)
        {
            switch (ch)
            {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                    result += buf;
                }
                else
                {
                    result += ch;
                }
                break;
            }
        }

        return result;
    }
}
