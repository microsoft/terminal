// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "SftpProfileStore.h"

#include <cwctype>

#include <cstdlib>

namespace
{
    // Returns the position of the closing quote for the string that starts at
    // `open`, skipping escaped characters, or npos if there is none.
    size_t FindJsonStringEnd(const std::wstring& text, size_t open)
    {
        size_t i{ open + 1 };
        while (i < text.size())
        {
            if (text[i] == L'\\')
            {
                i += 2;
                continue;
            }
            if (text[i] == L'"')
            {
                return i;
            }
            ++i;
        }
        return std::wstring::npos;
    }
}

namespace SftpProfileStore
{
    std::wstring JsonEscape(const std::wstring& in)
    {
        std::wstring out;
        out.reserve(in.size());
        for (const wchar_t ch : in)
        {
            switch (ch)
            {
            case L'\\':
                out += L"\\\\";
                break;
            case L'\"':
                out += L"\\\"";
                break;
            case L'\n':
                out += L"\\n";
                break;
            case L'\r':
                out += L"\\r";
                break;
            case L'\t':
                out += L"\\t";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    std::optional<std::wstring> JsonUnescape(const std::wstring& in)
    {
        std::wstring out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            if (in[i] != L'\\')
            {
                out.push_back(in[i]);
                continue;
            }
            if (i + 1 >= in.size())
            {
                return std::nullopt;
            }
            switch (in[++i])
            {
            case L'n':
                out.push_back(L'\n');
                break;
            case L'r':
                out.push_back(L'\r');
                break;
            case L't':
                out.push_back(L'\t');
                break;
            case L'b':
                out.push_back(L'\b');
                break;
            case L'f':
                out.push_back(L'\f');
                break;
            case L'u':
                if (i + 4 < in.size())
                {
                    wchar_t digits[5]{ in[i + 1], in[i + 2], in[i + 3], in[i + 4], L'\0' };
                    out.push_back(static_cast<wchar_t>(wcstoul(digits, nullptr, 16)));
                    i += 4;
                }
                else
                {
                    return std::nullopt;
                }
                break;
            default:
                out.push_back(in[i]);
                break; // covers escaped quotes and backslashes
            }
        }
        return out;
    }

    std::wstring SerializeProfiles(const std::vector<SftpConnectionProfile>& profiles)
    {
        std::wstring json{ L"[\n" };
        for (size_t i = 0; i < profiles.size(); ++i)
        {
            const auto& profile{ profiles[i] };
            json += L"  {\n";
            json += L"    \"name\": \"" + JsonEscape(profile.name) + L"\",\n";
            json += L"    \"host\": \"" + JsonEscape(profile.host) + L"\",\n";
            json += L"    \"port\": " + std::to_wstring(profile.port) + L",\n";
            json += L"    \"username\": \"" + JsonEscape(profile.username) + L"\",\n";
            json += L"    \"password\": \"" + JsonEscape(SftpPassword::Encrypt(profile.password)) + L"\",\n";
            json += L"    \"keyPath\": \"" + JsonEscape(profile.keyPath) + L"\"\n";
            json += (i + 1 == profiles.size()) ? L"  }\n" : L"  },\n";
        }
        json += L"]\n";
        return json;
    }

    bool DeserializeProfiles(const std::wstring& text, std::vector<SftpConnectionProfile>& out)
    {
        out.clear();

        size_t pos{ 0 };
        while (pos < text.size())
        {
            const auto openBrace{ text.find(L'{', pos) };
            if (openBrace == std::wstring::npos)
            {
                break;
            }
            pos = openBrace + 1;

            SftpConnectionProfile profile;
            bool closed{ false };
            while (!closed && pos < text.size())
            {
                const auto openQuote{ text.find(L'"', pos) };
                if (openQuote == std::wstring::npos)
                {
                    break;
                }
                const auto closeQuote{ FindJsonStringEnd(text, openQuote) };
                if (closeQuote == std::wstring::npos)
                {
                    break;
                }
                auto keyMaybe{ JsonUnescape(text.substr(openQuote + 1, closeQuote - openQuote - 1)) };
                if (!keyMaybe)
                {
                    break;
                }
                const auto key{ std::move(*keyMaybe) };
                pos = closeQuote + 1;

                const auto colon{ text.find(L':', pos) };
                if (colon == std::wstring::npos)
                {
                    break;
                }
                pos = colon + 1;

                while (pos < text.size() && iswspace(text[pos]))
                {
                    ++pos;
                }

                if (pos < text.size() && text[pos] == L'"')
                {
                    const auto valueEnd{ FindJsonStringEnd(text, pos) };
                    if (valueEnd == std::wstring::npos)
                    {
                        break;
                    }
                    auto valueMaybe{ JsonUnescape(text.substr(pos + 1, valueEnd - pos - 1)) };
                    if (!valueMaybe)
                    {
                        break;
                    }
                    auto value{ std::move(*valueMaybe) };
                    pos = valueEnd + 1;

                    if (key == L"name")
                    {
                        profile.name = std::move(value);
                    }
                    else if (key == L"host")
                    {
                        profile.host = std::move(value);
                    }
                    else if (key == L"username")
                    {
                        profile.username = std::move(value);
                    }
                    else if (key == L"password")
                    {
                        profile.password = SftpPassword::Decrypt(value);
                    }
                    else if (key == L"keyPath")
                    {
                        profile.keyPath = std::move(value);
                    }
                }
                else if (key == L"port")
                {
                    const auto numStart{ pos };
                    while (pos < text.size() && (iswdigit(text[pos]) || text[pos] == L'-'))
                    {
                        ++pos;
                    }
                    try
                    {
                        profile.port = static_cast<uint32_t>(std::stoul(text.substr(numStart, pos - numStart), nullptr, 10));
                    }
                    catch (...)
                    {
                        profile.port = 22;
                    }
                }

                while (!closed && pos < text.size())
                {
                    if (text[pos] == L',')
                    {
                        ++pos;
                        break;
                    }
                    if (text[pos] == L'}')
                    {
                        ++pos;
                        closed = true;
                        break;
                    }
                    ++pos;
                }
            }

            if (!profile.name.empty() || !profile.host.empty())
            {
                out.push_back(std::move(profile));
            }
        }

        return true;
    }
}