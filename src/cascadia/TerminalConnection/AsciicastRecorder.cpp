// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AsciicastRecorder.h"

#include <cmath>
#include <cstdio>
#include <ctime>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    void AsciicastRecorder::StartRecording(const ITerminalConnection& connection, const std::wstring& filePath, uint32_t width, uint32_t height, const AsciicastMetadata& metadata)
    {
        if (_isRecording.load(std::memory_order_acquire))
        {
            return;
        }

        _file.open(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!_file.is_open())
        {
            return;
        }

        _connection = connection;

        // Write the header synchronously before the channel exists (no contention).
        _WriteHeader(width, height, metadata);

        _lastEventTime = std::chrono::high_resolution_clock::now();
        _timingError = 0.0;
        _writeError.store(false, std::memory_order_relaxed);

        // Create the SPSC channel and start the writer thread.
        auto [tx, rx] = til::spsc::channel<AsciicastRecord>(4096);
        _channel = std::move(tx);
        _writerThread = std::thread([this, rx = std::move(rx)]() mutable {
            _WriterThread(std::move(rx));
        });

        _isRecording.store(true, std::memory_order_release);

        // Safe to capture `this`: ControlCore owns the recorder via unique_ptr
        // and calls StopRecording() (which revokes this token) before destruction.
        _outputToken = _connection.TerminalOutput([this](const winrt::array_view<const char16_t> data) {
            const std::wstring wstr{ data.begin(), data.end() };
            _WriteEvent(wstr);
        });
    }

    void AsciicastRecorder::StopRecording()
    {
        // Atomically transition from recording to not-recording. Only one
        // caller can succeed here; subsequent calls are no-ops.
        bool expected = true;
        if (!_isRecording.compare_exchange_strong(expected, false, std::memory_order_seq_cst))
        {
            return;
        }

        // Revoke the callback so no new invocations will be dispatched.
        _connection.TerminalOutput(_outputToken);
        _outputToken = {};
        _connection = nullptr;

        // An in-flight callback that already passed the _isRecording check may
        // still be computing its record and calling emplace. Spin until it
        // finishes so we can safely enqueue our exit event.
        while (_producerBusy.load(std::memory_order_seq_cst))
        {
            std::this_thread::yield();
        }

        // Compute the exit event interval from the last event.
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> interval = now - _lastEventTime;
        const double rawInterval = interval.count() + _timingError;
        const double roundedInterval = std::round(rawInterval * 1000.0) / 1000.0;

        _channel.emplace(AsciicastRecord::Type::Exit, roundedInterval, std::string("0"));

        // Close the channel by destroying the producer. The consumer loop
        // in _WriterThread will drain remaining records and then exit.
        { auto discard = std::move(_channel); }

        if (_writerThread.joinable())
        {
            _writerThread.join();
        }

        _file.close();
    }

    void AsciicastRecorder::WriteInitialSnapshot(const std::wstring_view& data)
    {
        if (!_isRecording.load(std::memory_order_seq_cst) || data.empty())
        {
            return;
        }

        _producerBusy.store(true, std::memory_order_seq_cst);
        if (!_isRecording.load(std::memory_order_seq_cst))
        {
            _producerBusy.store(false, std::memory_order_seq_cst);
            return;
        }

        const auto escaped = _EscapeJsonString(data);
        _channel.emplace(AsciicastRecord::Type::Output, 0.0, std::move(escaped));

        _producerBusy.store(false, std::memory_order_seq_cst);
    }

    void AsciicastRecorder::WriteResizeEvent(uint32_t cols, uint32_t rows)
    {
        if (!_isRecording.load(std::memory_order_seq_cst) || _writeError.load(std::memory_order_relaxed))
        {
            return;
        }

        _producerBusy.store(true, std::memory_order_seq_cst);
        if (!_isRecording.load(std::memory_order_seq_cst))
        {
            _producerBusy.store(false, std::memory_order_seq_cst);
            return;
        }

        const double roundedInterval = _ComputeInterval();

        char dataBuf[64];
        snprintf(dataBuf, sizeof(dataBuf), "%ux%u", cols, rows);

        _channel.emplace(AsciicastRecord::Type::Resize, roundedInterval, std::string(dataBuf));

        _producerBusy.store(false, std::memory_order_seq_cst);
    }

    void AsciicastRecorder::WriteMarkerEvent(const std::wstring_view& label)
    {
        if (!_isRecording.load(std::memory_order_seq_cst) || _writeError.load(std::memory_order_relaxed))
        {
            return;
        }

        _producerBusy.store(true, std::memory_order_seq_cst);
        if (!_isRecording.load(std::memory_order_seq_cst))
        {
            _producerBusy.store(false, std::memory_order_seq_cst);
            return;
        }

        const double roundedInterval = _ComputeInterval();
        const auto escaped = _EscapeJsonString(label);

        _channel.emplace(AsciicastRecord::Type::Marker, roundedInterval, std::move(escaped));

        _producerBusy.store(false, std::memory_order_seq_cst);
    }

    bool AsciicastRecorder::IsRecording() const noexcept
    {
        return _isRecording.load(std::memory_order_acquire);
    }

    void AsciicastRecorder::_WriteHeader(uint32_t width, uint32_t height, const AsciicastMetadata& metadata)
    {
        const auto timestamp = std::time(nullptr);

        _file << "{\"version\": 3, \"term\": {\"cols\": " << width
              << ", \"rows\": " << height;

        if (!metadata.termType.empty())
        {
            _file << ", \"type\": \"" << _EscapeJsonString(metadata.termType) << "\"";
        }

        // Write theme if we have at least fg and bg
        if (!metadata.themeFg.empty() && !metadata.themeBg.empty())
        {
            _file << ", \"theme\": {\"fg\": \"" << _EscapeJsonString(metadata.themeFg)
                  << "\", \"bg\": \"" << _EscapeJsonString(metadata.themeBg) << "\"";
            if (!metadata.themePalette.empty())
            {
                _file << ", \"palette\": \"" << _EscapeJsonString(metadata.themePalette) << "\"";
            }
            _file << "}";
        }

        _file << "}, \"timestamp\": " << timestamp;

        if (!metadata.title.empty())
        {
            _file << ", \"title\": \"" << _EscapeJsonString(metadata.title) << "\"";
        }

        if (!metadata.command.empty())
        {
            _file << ", \"command\": \"" << _EscapeJsonString(metadata.command) << "\"";
        }

        _file << "}\n";
        _file.flush();
    }

    void AsciicastRecorder::_WriteEvent(const std::wstring_view& data)
    {
        if (!_isRecording.load(std::memory_order_seq_cst) || _writeError.load(std::memory_order_relaxed))
        {
            return;
        }

        // Mark that we are about to emplace. StopRecording spins on this
        // flag so it does not destroy the producer while we are using it.
        _producerBusy.store(true, std::memory_order_seq_cst);

        // Double-check: if StopRecording set _isRecording=false between
        // our first load and the store above, bail out immediately.
        if (!_isRecording.load(std::memory_order_seq_cst))
        {
            _producerBusy.store(false, std::memory_order_seq_cst);
            return;
        }

        const double roundedInterval = _ComputeInterval();
        const auto escaped = _EscapeJsonString(data);

        _channel.emplace(AsciicastRecord::Type::Output, roundedInterval, std::move(escaped));

        _producerBusy.store(false, std::memory_order_seq_cst);
    }

    double AsciicastRecorder::_ComputeInterval()
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> interval = now - _lastEventTime;
        _lastEventTime = now;

        // Error diffusion: carry over sub-millisecond rounding remainder to keep
        // total elapsed time accurate over long recordings (v3 spec recommendation).
        const double rawInterval = interval.count() + _timingError;
        const double roundedInterval = std::round(rawInterval * 1000.0) / 1000.0;
        _timingError = rawInterval - roundedInterval;

        return roundedInterval;
    }

    void AsciicastRecorder::_WriterThread(til::spsc::consumer<AsciicastRecord> rx)
    {
        std::optional<AsciicastRecord> item;
        while ((item = rx.pop()))
        {
            auto& record = *item;

            char timeBuf[32];
            snprintf(timeBuf, sizeof(timeBuf), "%.3f", record.interval);

            const char typeChar = static_cast<char>(record.type);
            _file << "[" << timeBuf << ", \"" << typeChar << "\", \"" << record.data << "\"]\n";
            _file.flush();

            if (!_file.good())
            {
                _writeError.store(true, std::memory_order_release);
                return;
            }
        }
        // Channel closed -- all records have been drained.
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
