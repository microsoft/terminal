// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Extracted pure functions from wtcli for fuzzing and testability.
// These functions have no COM/WinRT dependencies and can be called
// from a LibFuzzer harness.

#pragma once

#include <string>
#include <sstream>
#include <vector>

#include <Windows.h>
#include <json/json.h>

namespace wtcli
{
    // Concatenate positional args as literal UTF-8 → UTF-16 text without
    // any tmux-style token interpretation. Use this when the caller's intent
    // is "send these exact characters" (e.g. wta forwarding agent-supplied
    // text), so payloads like the literal word "Enter" / "Tab" / "C-c" are
    // not silently rewritten into control bytes.
    inline std::wstring JoinAsUtf16(const std::vector<std::string>& parts)
    {
        std::wstring result;
        bool first = true;
        for (const auto& p : parts)
        {
            // Space-separate consecutive args so an unquoted human invocation
            // like `wtcli send-keys --raw hello world` reaches the pane as
            // "hello world" rather than "helloworld". wta callers pass a
            // single positional via `--`, so they are unaffected.
            if (!first)
            {
                result += L' ';
            }
            first = false;
            if (p.empty())
                continue;
            const int wlen = MultiByteToWideChar(CP_UTF8, 0, p.data(), static_cast<int>(p.size()), nullptr, 0);
            if (wlen > 0)
            {
                const size_t prev = result.size();
                result.resize(prev + static_cast<size_t>(wlen));
                MultiByteToWideChar(CP_UTF8, 0, p.data(), static_cast<int>(p.size()), result.data() + prev, wlen);
            }
        }
        return result;
    }

    // Translate tmux-style key names to the byte stream that should be sent
    // to a pane. Recognized tokens: Enter / Space / Tab / Escape (alias Esc) /
    // BSpace / C-a..C-z. Unrecognized tokens are passed through as UTF-8 →
    // UTF-16 text. "Enter" maps to a single CR — SendProtocolInput downstream
    // translates LF to CR as well, so emitting CRLF here would produce a
    // double-CR (two Enter keypresses).
    inline std::wstring TranslateKeys(const std::vector<std::string>& keys)
    {
        std::wstring result;
        for (const auto& key : keys)
        {
            if (key == "Enter" || key == "enter")
                result += L"\r";
            else if (key == "Space" || key == "space")
                result += L" ";
            else if (key == "Tab" || key == "tab")
                result += L"\t";
            else if (key == "Escape" || key == "escape" || key == "Esc" || key == "esc")
                result += L"\x1b";
            else if (key == "BSpace" || key == "bspace")
                result += L"\b";
            else if (key == "C-c")
                result += L"\x03";
            else if (key == "C-d")
                result += L"\x04";
            else if (key == "C-z")
                result += L"\x1a";
            else if (key == "C-l")
                result += L"\x0c";
            else if (key.size() == 3 && key[0] == 'C' && key[1] == '-' && key[2] >= 'a' && key[2] <= 'z')
                result += static_cast<wchar_t>(key[2] - 'a' + 1);
            else if (!key.empty())
            {
                const int wlen = MultiByteToWideChar(CP_UTF8, 0, key.data(), static_cast<int>(key.size()), nullptr, 0);
                if (wlen > 0)
                {
                    const size_t prev = result.size();
                    result.resize(prev + static_cast<size_t>(wlen));
                    MultiByteToWideChar(CP_UTF8, 0, key.data(), static_cast<int>(key.size()), result.data() + prev, wlen);
                }
            }
        }
        return result;
    }

}
