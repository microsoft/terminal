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
    // iTerm2's tmux -CC gateway banner. WezTerm prints the same text.
    inline constexpr std::string_view TmuxCommandMenu{
        "\r\n** tmux mode started **\r\n\r\n"
        "Command Menu\r\n"
        "----------------------------\r\n"
        "esc    Detach cleanly.\r\n"
        "  X    Force-quit tmux mode.\r\n"
        "  L    Toggle logging.\r\n"
        "  C    Run tmux command.\r\n"
    };
    // ConPTY strips DCS. EternalTerminal's Windows htm client carries control
    // bytes as CSI ?777;b0;b1;...q (at most 15 payload bytes per sequence).
    inline constexpr std::string_view ConPtyHtmCarrierPrefix{ "\x1b[?777" };
    inline size_t LongestInitPrefix(std::string_view data, std::string_view needle);

    inline std::string EncodeConPtyHtmCarrier(std::string_view bytes)
    {
        std::string out;
        constexpr size_t chunkSize = 15;
        for (size_t offset = 0; offset < bytes.size(); offset += chunkSize)
        {
            const auto end = std::min(bytes.size(), offset + chunkSize);
            out.append(ConPtyHtmCarrierPrefix);
            for (size_t i = offset; i < end; ++i)
            {
                out.push_back(';');
                out += std::to_string(static_cast<unsigned char>(bytes[i]));
            }
            out.push_back('q');
        }
        return out;
    }

    struct CarrierDecodeResult
    {
        std::string decoded;
        std::string pending;
    };

    inline CarrierDecodeResult DecodeConPtyHtmCarrier(std::string_view pending, std::string_view incoming)
    {
        std::string data;
        data.reserve(pending.size() + incoming.size());
        data.append(pending);
        data.append(incoming);
        CarrierDecodeResult result;
        size_t i = 0;
        while (i < data.size())
        {
            const auto pos = data.find(ConPtyHtmCarrierPrefix, i);
            if (pos == std::string::npos)
            {
                const auto keep = LongestInitPrefix(std::string_view{ data }.substr(i), ConPtyHtmCarrierPrefix);
                result.decoded.append(data.substr(i, data.size() - i - keep));
                result.pending = data.substr(data.size() - keep);
                return result;
            }
            result.decoded.append(data.substr(i, pos - i));
            size_t cursor = pos + ConPtyHtmCarrierPrefix.size();
            std::string payload;
            bool complete = false;
            bool invalid = false;
            while (cursor < data.size())
            {
                if (data[cursor] == 'q')
                {
                    complete = true;
                    ++cursor;
                    break;
                }
                if (data[cursor] != ';')
                {
                    invalid = true;
                    break;
                }
                ++cursor;
                if (cursor >= data.size())
                {
                    break;
                }
                if (data[cursor] < '0' || data[cursor] > '9')
                {
                    invalid = true;
                    break;
                }
                int value = 0;
                while (cursor < data.size() && data[cursor] >= '0' && data[cursor] <= '9')
                {
                    value = value * 10 + (data[cursor] - '0');
                    ++cursor;
                }
                payload.push_back(static_cast<char>(value & 0xFF));
            }
            if (invalid)
            {
                result.decoded.push_back(data[pos]);
                i = pos + 1;
                continue;
            }
            if (!complete)
            {
                result.pending = data.substr(pos);
                return result;
            }
            result.decoded.append(payload);
            i = cursor;
        }
        return result;
    }

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

    // Encode a Unicode code point as UTF-8 (rejects surrogates / out-of-range).
    inline void AppendUtf8CodePoint(std::string& out, char32_t cp)
    {
        if (cp < 0x80)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0xD800 || (cp > 0xDFFF && cp < 0x10000))
        {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0x10FFFF)
        {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // KEYEVENTF_UNICODE may deliver one UTF-16 code unit per win32-input-mode
    // record; hold an unpaired high surrogate across DecodeWin32InputMode calls.
    struct Win32InputDecodeState
    {
        char16_t pendingHigh{};
    };

    // Windows Terminal's win32-input-mode: ESC [ vk ; sc ; uc ; kd ; cs ; rc _
    inline std::string DecodeWin32InputMode(std::string_view utf8, Win32InputDecodeState& state)
    {
        std::string out;
        size_t i = 0;
        while (i < utf8.size())
        {
            if (utf8.size() - i >= 2 && utf8[i] == '\x1b' && utf8[i + 1] == '[')
            {
                const auto end = utf8.find('_', i + 2);
                if (end != std::string_view::npos)
                {
                    const auto body = utf8.substr(i + 2, end - (i + 2));
                    int fields[6] = {};
                    int count = 0;
                    size_t p = 0;
                    while (p < body.size() && count < 6)
                    {
                        int value = 0;
                        while (p < body.size() && body[p] >= '0' && body[p] <= '9')
                        {
                            value = value * 10 + (body[p] - '0');
                            ++p;
                        }
                        fields[count++] = value;
                        if (p < body.size() && body[p] == ';')
                        {
                            ++p;
                        }
                    }
                    i = end + 1;
                    if (count >= 4)
                    {
                        const int vk = fields[0];
                        const int uc = fields[2];
                        const int keyDown = fields[3];
                        if (keyDown != 1)
                        {
                            continue;
                        }
                        if (uc > 0)
                        {
                            const auto unit = static_cast<char32_t>(uc);
                            if (unit >= 0xD800 && unit <= 0xDBFF)
                            {
                                state.pendingHigh = static_cast<char16_t>(unit);
                                continue;
                            }
                            if (unit >= 0xDC00 && unit <= 0xDFFF)
                            {
                                if (state.pendingHigh)
                                {
                                    const char32_t cp = 0x10000 +
                                                       ((static_cast<char32_t>(state.pendingHigh) - 0xD800) << 10) +
                                                       (unit - 0xDC00);
                                    state.pendingHigh = 0;
                                    AppendUtf8CodePoint(out, cp);
                                }
                                continue;
                            }
                            state.pendingHigh = 0;
                            AppendUtf8CodePoint(out, unit);
                        }
                        else if (vk == 0x0D)
                        {
                            out.push_back('\r');
                        }
                        else if (vk == 0x08)
                        {
                            out.push_back('\x7f');
                        }
                        else if (vk == 0x1B)
                        {
                            out.push_back('\x1b');
                        }
                    }
                    continue;
                }
            }
            out.append(utf8.substr(i));
            break;
        }
        return out;
    }

    inline std::string DecodeWin32InputMode(std::string_view utf8)
    {
        Win32InputDecodeState state;
        return DecodeWin32InputMode(utf8, state);
    }

    // Collect pane ids from a tmux window_layout body (checksum optional).
    // Leaves look like WxH,X,Y,id; splits use { } / [ ] and are skipped.
    inline std::vector<std::string> PaneIdsFromTmuxLayout(std::string_view layout)
    {
        std::vector<std::string> ids;
        size_t i = 0;
        while (i < layout.size())
        {
            // Find "NxM," size prefix.
            const auto xPos = layout.find('x', i);
            if (xPos == std::string_view::npos || xPos == i)
            {
                break;
            }
            bool digitsBefore = true;
            for (size_t j = i; j < xPos; ++j)
            {
                if (layout[j] < '0' || layout[j] > '9')
                {
                    digitsBefore = false;
                    break;
                }
            }
            if (!digitsBefore)
            {
                ++i;
                continue;
            }
            size_t p = xPos + 1;
            auto readNum = [&](size_t& pos) -> bool {
                if (pos >= layout.size() || layout[pos] < '0' || layout[pos] > '9')
                {
                    return false;
                }
                while (pos < layout.size() && layout[pos] >= '0' && layout[pos] <= '9')
                {
                    ++pos;
                }
                return true;
            };
            if (!readNum(p) || p >= layout.size() || layout[p] != ',')
            {
                i = xPos + 1;
                continue;
            }
            ++p; // X
            if (!readNum(p) || p >= layout.size() || layout[p] != ',')
            {
                i = xPos + 1;
                continue;
            }
            ++p; // Y
            if (!readNum(p) || p >= layout.size())
            {
                i = xPos + 1;
                continue;
            }
            if (layout[p] == ',' )
            {
                ++p;
                const size_t idStart = p;
                if (!readNum(p) || idStart == p)
                {
                    i = xPos + 1;
                    continue;
                }
                ids.push_back("%" + std::string{ layout.substr(idStart, p - idStart) });
                i = p;
                continue;
            }
            // Split container: WxH,X,Y{...} or [...]
            i = p;
        }
        return ids;
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
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
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
