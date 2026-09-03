// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// HTM (headless terminal multiplexer) wire protocol, matching
// EternalTerminal HtmHeaderCodes and hyper-htm/htm-core.js.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Microsoft::Terminal::Htm
{
    // HTM now uses tmux control mode. These are terminal-facing markers; the
    // bytes after DCS are ordinary newline-delimited tmux control records.
    inline constexpr std::string_view TmuxControlDcs{ "\x1bP1000p" };
    inline constexpr std::string_view TmuxControlSt{ "\x1b\\" };

    inline std::string UnescapeControlOutput(std::string_view input)
    {
        std::string result;
        result.reserve(input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] == '\\' && i + 3 < input.size() &&
                input[i + 1] >= '0' && input[i + 1] <= '7' &&
                input[i + 2] >= '0' && input[i + 2] <= '7' &&
                input[i + 3] >= '0' && input[i + 3] <= '7')
            {
                result.push_back(static_cast<char>(((input[i + 1] - '0') << 6) |
                                                   ((input[i + 2] - '0') << 3) |
                                                   (input[i + 3] - '0')));
                i += 3;
            }
            else
            {
                result.push_back(input[i]);
            }
        }
        return result;
    }

    inline constexpr char InsertKeys = '1';
    inline constexpr char InitState = '2';
    inline constexpr char ClientClosePane = '3';
    inline constexpr char AppendToPane = '4';
    inline constexpr char NewTab = '5';
    inline constexpr char ServerClosePane = '8';
    inline constexpr char NewSplit = '9';
    inline constexpr char ResizePane = 'A';
    inline constexpr char DebugLog = 'B';
    inline constexpr char InsertDebugKeys = 'C';
    inline constexpr char SessionEnd = 'D';

    inline constexpr size_t UuidLength = 36;
    inline constexpr std::string_view InitSequence{ "\x1b[###q" };
    inline constexpr std::string_view ExitSequence{ "\x1b[$$$q" };

    inline constexpr char VerticalSplit = '1';
    inline constexpr char HorizontalSplit = '0';

    struct Packet
    {
        char header{};
        std::string payload;
        bool invalidLength{ false };
    };

    inline std::string Base64Encode(const void* data, size_t size)
    {
        static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::string out;
        out.reserve(((size + 2) / 3) * 4);
        for (size_t i = 0; i < size; i += 3)
        {
            const unsigned int b0 = bytes[i];
            const unsigned int b1 = i + 1 < size ? bytes[i + 1] : 0;
            const unsigned int b2 = i + 2 < size ? bytes[i + 2] : 0;
            const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
            out.push_back(kTable[(triple >> 18) & 0x3F]);
            out.push_back(kTable[(triple >> 12) & 0x3F]);
            out.push_back(i + 1 < size ? kTable[(triple >> 6) & 0x3F] : '=');
            out.push_back(i + 2 < size ? kTable[triple & 0x3F] : '=');
        }
        return out;
    }

    inline std::string Base64Decode(std::string_view encoded)
    {
        static constexpr signed char kDecode[256] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
            -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        };
        std::string out;
        int val = 0;
        int valueBits = -8;
        for (unsigned char c : encoded)
        {
            if (c == '=')
            {
                break;
            }
            const signed char d = kDecode[c];
            if (d < 0)
            {
                continue;
            }
            val = (val << 6) + d;
            valueBits += 6;
            if (valueBits >= 0)
            {
                out.push_back(char((val >> valueBits) & 0xFF));
                valueBits -= 8;
            }
        }
        return out;
    }

    inline std::string EncodeLength(int32_t length)
    {
        return Base64Encode(&length, sizeof(length));
    }

    inline int32_t DecodeLength(std::string_view b64)
    {
        const auto bytes = Base64Decode(b64.substr(0, 8));
        if (bytes.size() < 4)
        {
            return -1;
        }
        int32_t value = 0;
        memcpy(&value, bytes.data(), 4);
        return value;
    }

    inline std::string FramePacket(char header, std::string_view payload)
    {
        std::string out;
        out.reserve(9 + payload.size());
        out.push_back(header);
        out += EncodeLength(static_cast<int32_t>(payload.size()));
        out.append(payload.data(), payload.size());
        return out;
    }

    inline std::string FrameInsertKeys(std::string_view paneId, std::string_view utf8Keys)
    {
        const auto encoded = Base64Encode(utf8Keys.data(), utf8Keys.size());
        std::string payload;
        payload.reserve(paneId.size() + encoded.size());
        payload.append(paneId);
        payload.append(encoded);
        return FramePacket(InsertKeys, payload);
    }

    inline std::string FrameInsertDebugKeys(std::string_view keys)
    {
        return FramePacket(InsertDebugKeys, keys);
    }

    inline std::string FrameNewTab(std::string_view tabId, std::string_view paneId)
    {
        std::string payload;
        payload.append(tabId);
        payload.append(paneId);
        return FramePacket(NewTab, payload);
    }

    inline std::string FrameNewSplit(std::string_view sourceId, std::string_view paneId, bool vertical)
    {
        std::string payload;
        payload.append(sourceId);
        payload.append(paneId);
        payload.push_back(vertical ? VerticalSplit : HorizontalSplit);
        return FramePacket(NewSplit, payload);
    }

    inline std::string FrameResizePane(std::string_view paneId, int32_t cols, int32_t rows)
    {
        std::string payload = Base64Encode(&cols, 4) + Base64Encode(&rows, 4);
        payload.append(paneId);
        return FramePacket(ResizePane, payload);
    }

    inline std::string FrameClientClosePane(std::string_view paneId)
    {
        return FramePacket(ClientClosePane, paneId);
    }

    inline size_t LongestInitPrefix(std::string_view data, std::string_view needle)
    {
        const auto max = std::min(data.size(), needle.size() - 1);
        for (size_t n = max; n > 0; --n)
        {
            if (needle.substr(0, n) == data.substr(data.size() - n))
            {
                return n;
            }
        }
        return 0;
    }

    struct ConsumeInitResult
    {
        bool matched{ false };
        std::string prefix;
        std::string remainder;
        std::string pending;
    };

    inline ConsumeInitResult ConsumeInitPayload(std::string_view pending, std::string_view payload, std::string_view needle = InitSequence)
    {
        std::string data;
        data.reserve(pending.size() + payload.size());
        data.append(pending);
        data.append(payload);
        const auto initAt = data.find(needle);
        if (initAt != std::string::npos)
        {
            return { true, data.substr(0, initAt), data.substr(initAt + needle.size()), {} };
        }
        const auto hold = LongestInitPrefix(data, needle);
        if (hold > 0)
        {
            return { false, data.substr(0, data.size() - hold), {}, data.substr(data.size() - hold) };
        }
        return { false, data, {}, {} };
    }

    inline std::pair<std::vector<Packet>, std::string> ParsePackets(std::string_view buffer)
    {
        std::vector<Packet> packets;
        size_t offset = 0;
        while (offset < buffer.size())
        {
            const char header = buffer[offset];
            if (header == SessionEnd)
            {
                packets.push_back({ header, {}, false });
                offset += 1;
                break;
            }
            if (buffer.size() - offset < 9)
            {
                break;
            }
            const auto length = DecodeLength(buffer.substr(offset + 1, 8));
            if (length < 0)
            {
                packets.push_back({ header, {}, true });
                break;
            }
            if (buffer.size() - offset - 9 < static_cast<size_t>(length))
            {
                break;
            }
            packets.push_back({ header, std::string(buffer.substr(offset + 9, static_cast<size_t>(length))), false });
            offset += 9 + static_cast<size_t>(length);
        }
        return { std::move(packets), std::string(buffer.substr(offset)) };
    }
}
