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
        _autoResize = unbox_prop_or<bool>(settings, L"AutoResizeForPlayback", false);
        _parseFile();
    }

    void AsciicastConnection::Start()
    {
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
        _generation.fetch_add(1);
        _paused.store(false); // unblock if paused
        _transitionToState(ConnectionState::Closed);
    }

    void AsciicastConnection::PausePlayback()
    {
        if (!_paused.exchange(true))
        {
            PlaybackStateChanged.raise(*this, nullptr);
        }
    }

    void AsciicastConnection::ResumePlayback()
    {
        if (_paused.exchange(false))
        {
            PlaybackStateChanged.raise(*this, nullptr);
        }
    }

    void AsciicastConnection::SeekPlayback(double position)
    {
        // Find the event index closest to the requested position.
        size_t targetIdx = 0;
        for (size_t i = 0; i < _events.size(); ++i)
        {
            if (_events[i].cumulativeTime > position)
            {
                break;
            }
            targetIdx = i;
        }
        _seekTarget.store(targetIdx);
    }

    void AsciicastConnection::RestartPlayback()
    {
        if (_playbackFinished.load())
        {
            // Playback ended, need to re-launch the coroutine.
            _generation.fetch_add(1);
            _playbackFinished.store(false);
            _cancelled.store(false);
            _paused.store(false);
            _currentPosition.store(0.0);
            _currentEventIndex = 0;
            _seekTarget.store(SIZE_MAX);
            PlaybackStateChanged.raise(*this, nullptr);
            _replayEvents();
        }
        else
        {
            SeekPlayback(0.0);
        }
    }

    void AsciicastConnection::_emitAllOutputUpTo(size_t endIndex)
    {
        // Clear the terminal and replay all output events up to endIndex.
        static constexpr std::wstring_view clearSeq{ L"\x1b[2J\x1b[H\x1b[0m" };
        TerminalOutput.raise(winrt_wstring_to_array_view(clearSeq));

        for (size_t i = 0; i <= endIndex && i < _events.size(); ++i)
        {
            if (_events[i].type == L"o")
            {
                TerminalOutput.raise(winrt_wstring_to_array_view(_events[i].data));
            }
        }
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
                // Parse idle_time_limit if present
                auto idlePos = line.find("\"idle_time_limit\"");
                if (idlePos != std::string::npos)
                {
                    auto colonPos = line.find(':', idlePos + 17);
                    if (colonPos != std::string::npos)
                    {
                        try
                        {
                            _idleTimeLimit = std::stod(line.substr(colonPos + 1));
                        }
                        catch (...)
                        {
                            // Keep default
                        }
                    }
                }

                // Parse terminal dimensions from header.
                // v3: "term": {"cols": X, "rows": Y}
                // v2: "width": X, "height": Y
                if (_formatVersion >= 3)
                {
                    auto colsPos = line.find("\"cols\"");
                    if (colsPos != std::string::npos)
                    {
                        auto colonPos = line.find(':', colsPos + 6);
                        if (colonPos != std::string::npos)
                        {
                            try
                            {
                                _initialCols = static_cast<uint32_t>(std::stoi(line.substr(colonPos + 1)));
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                    auto rowsPos = line.find("\"rows\"");
                    if (rowsPos != std::string::npos)
                    {
                        auto colonPos = line.find(':', rowsPos + 6);
                        if (colonPos != std::string::npos)
                        {
                            try
                            {
                                _initialRows = static_cast<uint32_t>(std::stoi(line.substr(colonPos + 1)));
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }
                else
                {
                    auto widthPos = line.find("\"width\"");
                    if (widthPos != std::string::npos)
                    {
                        auto colonPos = line.find(':', widthPos + 7);
                        if (colonPos != std::string::npos)
                        {
                            try
                            {
                                _initialCols = static_cast<uint32_t>(std::stoi(line.substr(colonPos + 1)));
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                    auto heightPos = line.find("\"height\"");
                    if (heightPos != std::string::npos)
                    {
                        auto colonPos = line.find(':', heightPos + 8);
                        if (colonPos != std::string::npos)
                        {
                            try
                            {
                                _initialRows = static_cast<uint32_t>(std::stoi(line.substr(colonPos + 1)));
                            }
                            catch (...)
                            {
                            }
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

            _events.push_back({ timestamp, 0.0, std::wstring{ wType }, wData });
        }

        // Compute cumulative timestamps and total duration.
        double cumulative = 0.0;
        double prevTs = 0.0;
        for (auto& evt : _events)
        {
            const auto interval = (_formatVersion >= 3) ? evt.timestamp : (evt.timestamp - prevTs);
            prevTs = evt.timestamp;
            cumulative += std::max(0.0, std::min(interval, _idleTimeLimit));
            evt.cumulativeTime = cumulative;
        }
        _totalDuration = cumulative;
    }

    winrt::fire_and_forget AsciicastConnection::_replayEvents()
    {
        auto strongThis{ get_strong() };
        co_await winrt::resume_background();

        const auto myGeneration = _generation.load();

        _transitionToState(ConnectionState::Connected);

        // If auto-resize is enabled and we have header dimensions, emit a VT
        // XTWINOPS sequence (CSI 8;rows;cols t) to resize the terminal to match
        // the recording. The terminal's existing VT parser handles this natively.
        if (_autoResize && _initialCols > 0 && _initialRows > 0)
        {
            // Wait for terminal init — the VT parser needs time to connect
            // before it can process the resize sequence.
            co_await winrt::resume_after(std::chrono::milliseconds{ 100 });
            wchar_t resizeSeq[64];
            swprintf_s(resizeSeq, L"\x1b[8;%u;%ut", _initialRows, _initialCols);
            const std::wstring_view sv{ resizeSeq };
            TerminalOutput.raise(winrt_wstring_to_array_view(sv));
            // Wait for resize reflow — the terminal needs time to reflow
            // content after changing dimensions.
            co_await winrt::resume_after(std::chrono::milliseconds{ 200 });
        }

        size_t i = 0;
        while (i < _events.size())
        {
            if (_cancelled.load() || _generation.load() != myGeneration)
            {
                break;
            }

            // Handle seek requests.
            const auto seekIdx = _seekTarget.exchange(SIZE_MAX);
            if (seekIdx != SIZE_MAX && !_events.empty())
            {
                const auto clampedIdx = std::min(seekIdx, _events.size() - 1);
                _emitAllOutputUpTo(clampedIdx);
                _currentPosition.store(_events[clampedIdx].cumulativeTime);
                PlaybackPositionChanged.raise(*this, nullptr);
                i = clampedIdx;
                continue;
            }

            // Handle pause - poll with short sleeps to stay responsive.
            while (_paused.load() && !_cancelled.load() && _generation.load() == myGeneration)
            {
                // Check for seek while paused.
                const auto postSeek = _seekTarget.exchange(SIZE_MAX);
                if (postSeek != SIZE_MAX && !_events.empty())
                {
                    const auto clampedPost = std::min(postSeek, _events.size() - 1);
                    _emitAllOutputUpTo(clampedPost);
                    _currentPosition.store(_events[clampedPost].cumulativeTime);
                    PlaybackPositionChanged.raise(*this, nullptr);
                    i = clampedPost;
                }
                co_await winrt::resume_after(std::chrono::milliseconds{ 50 });
            }

            const auto& event = _events[i];

            // Compute delay with speed factor.
            const auto interval = (i == 0) ? 0.0 :
                (event.cumulativeTime - _events[i - 1].cumulativeTime);
            const auto speed = _playbackSpeed.load();
            if (interval > 0.0 && speed > 0.0)
            {
                const auto delayMs = static_cast<int64_t>((interval / speed) * 1000.0);
                if (delayMs > 0)
                {
                    co_await winrt::resume_after(std::chrono::milliseconds{ delayMs });
                }
            }

            if (_cancelled.load() || _generation.load() != myGeneration)
            {
                break;
            }

            // Emit the event.
            if (event.type == L"o")
            {
                TerminalOutput.raise(winrt_wstring_to_array_view(event.data));
            }
            else if (event.type == L"r" && _autoResize)
            {
                const auto xPos = event.data.find(L'x');
                if (xPos != std::wstring::npos)
                {
                    try
                    {
                        const auto cols = static_cast<uint32_t>(std::stoi(event.data.substr(0, xPos)));
                        const auto rows = static_cast<uint32_t>(std::stoi(event.data.substr(xPos + 1)));
                        wchar_t resizeSeq[64];
                        swprintf_s(resizeSeq, L"\x1b[8;%u;%ut", rows, cols);
                        const std::wstring_view sv{ resizeSeq };
                        TerminalOutput.raise(winrt_wstring_to_array_view(sv));
                    }
                    catch (...)
                    {
                    }
                }
            }

            // Update position.
            _currentPosition.store(event.cumulativeTime);
            _currentEventIndex = i;
            PlaybackPositionChanged.raise(*this, nullptr);

            ++i;
        }

        if (!_cancelled.load() && _generation.load() == myGeneration)
        {
            // Wait before showing completion overlay so the final frame
            // remains visible before the message appears.
            co_await winrt::resume_after(std::chrono::milliseconds{ 500 });

            static constexpr std::wstring_view completionMsg{
                L"\x1b[999;1H\r\n\x1b[1;36m--- Playback complete. Press any key to close. ---\x1b[0m"
            };
            TerminalOutput.raise(winrt_wstring_to_array_view(completionMsg));
            _playbackFinished.store(true);
            _paused.store(true);
            PlaybackStateChanged.raise(*this, nullptr);
        }
        else
        {
            _transitionToState(ConnectionState::Closed);
        }
    }
}
