// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AsciicastConnection.h"

#include "AsciicastConnection.g.cpp"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    AsciicastConnection::AsciicastConnection() noexcept = default;

    void AsciicastConnection::Initialize(const Windows::Foundation::Collections::ValueSet& settings)
    {
        _filePath = unbox_prop_or<winrt::hstring>(settings, L"CastFilePath", winrt::hstring{});
    }

    void AsciicastConnection::Start()
    {
        _parseFile();
        _replayEvents();
    }

    void AsciicastConnection::WriteInput(const winrt::array_view<const char16_t> /*buffer*/)
    {
        if (_playbackFinished.load())
        {
            // Any input after playback finishes closes the connection.
            Close();
        }
    }

    void AsciicastConnection::Resize(uint32_t /*rows*/, uint32_t /*columns*/) noexcept
    {
    }

    void AsciicastConnection::Close() noexcept
    {
        _cancelled.store(true);
        _transitionToState(ConnectionState::Closed);
    }

    // Unescape a JSON string value, handling standard JSON escape sequences.
    std::wstring AsciicastConnection::_unescapeJsonString(std::wstring_view input)
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

    // Parse an asciicast v2 or v3 file.
    // v2: timestamps are absolute. v3: timestamps are intervals.
    void AsciicastConnection::_parseFile()
    {
        _events.clear();

        const std::filesystem::path filePath{ std::wstring_view{ _filePath } };
        std::ifstream file{ filePath, std::ios::in | std::ios::binary };
        if (!file.is_open())
        {
            return;
        }

        std::string line;
        bool isFirstLine = true;

        while (std::getline(file, line))
        {
            // Trim trailing \r (Windows line endings).
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.empty())
            {
                continue;
            }

            // v3: skip comment lines.
            if (line[0] == '#')
            {
                continue;
            }

            if (isFirstLine)
            {
                // Detect format version from header.
                auto versionPos = line.find("\"version\"");
                if (versionPos != std::string::npos)
                {
                    auto colonPos = line.find(':', versionPos + 9);
                    if (colonPos != std::string::npos)
                    {
                        try
                        {
                            _formatVersion = std::stoi(line.substr(colonPos + 1));
                        }
                        catch (...)
                        {
                            _formatVersion = 2;
                        }
                    }
                }
                isFirstLine = false;
                continue;
            }

            // Parse event: [timestamp, "type", "data"]
            const auto bracketPos = line.find('[');
            if (bracketPos == std::string::npos)
            {
                continue;
            }

            const auto firstComma = line.find(',', bracketPos + 1);
            if (firstComma == std::string::npos)
            {
                continue;
            }

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
            {
                continue;
            }
            const auto typeQuote2 = line.find('"', typeQuote1 + 1);
            if (typeQuote2 == std::string::npos)
            {
                continue;
            }
            const auto type = line.substr(typeQuote1 + 1, typeQuote2 - typeQuote1 - 1);

            // Parse data field.
            const auto commaAfterType = line.find(',', typeQuote2 + 1);
            if (commaAfterType == std::string::npos)
            {
                continue;
            }

            const auto dataQuote1 = line.find('"', commaAfterType + 1);
            if (dataQuote1 == std::string::npos)
            {
                continue;
            }

            // Find matching close quote, skipping escapes.
            size_t dataQuote2 = std::string::npos;
            for (size_t i = dataQuote1 + 1; i < line.size(); ++i)
            {
                if (line[i] == '\\')
                {
                    ++i;
                    if (i >= line.size())
                        break;
                    continue;
                }
                if (line[i] == '"')
                {
                    dataQuote2 = i;
                    break;
                }
            }
            if (dataQuote2 == std::string::npos)
            {
                continue;
            }

            const auto rawData = line.substr(dataQuote1 + 1, dataQuote2 - dataQuote1 - 1);

            // Convert narrow strings to wide strings and unescape.
            const auto wType = til::u8u16(type);
            const auto wRawData = til::u8u16(rawData);
            const auto wData = _unescapeJsonString(wRawData);

            _events.push_back({ timestamp, std::wstring{ wType }, wData });
        }
    }

    winrt::fire_and_forget AsciicastConnection::_replayEvents()
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();

        _transitionToState(ConnectionState::Connected);

        double prevTimestamp = 0.0;

        for (const auto& event : _events)
        {
            if (_cancelled.load())
            {
                break;
            }

            // Only replay output events.
            if (event.type != L"o")
            {
                continue;
            }

            // v2: absolute timestamps, compute delta. v3: intervals, use directly.
            const auto delay = (_formatVersion >= 3) ? event.timestamp : (event.timestamp - prevTimestamp);
            if (delay > 0.0)
            {
                // Cap at 5 minutes to guard against malformed files.
                const auto cappedDelay = std::min(delay, 300.0);
                const auto delayMs = static_cast<int64_t>(cappedDelay * 1000.0);
                co_await winrt::resume_after(std::chrono::milliseconds{ delayMs });
            }

            if (_cancelled.load())
            {
                break;
            }

            TerminalOutput.raise(winrt_wstring_to_array_view(event.data));
            prevTimestamp = event.timestamp;
        }

        if (!_cancelled.load())
        {
            // Brief pause so the final frame is visible before we clear.
            co_await winrt::resume_after(std::chrono::milliseconds{ 500 });

            // Move to bottom of viewport and show message below content.
            static constexpr std::wstring_view completionMsg{
                L"\x1b[999;1H\r\n\x1b[1;36m--- Playback complete. Press any key to close. ---\x1b[0m"
            };
            TerminalOutput.raise(winrt_wstring_to_array_view(completionMsg));
            _playbackFinished.store(true);
        }
        else
        {
            _transitionToState(ConnectionState::Closed);
        }
    }
}
