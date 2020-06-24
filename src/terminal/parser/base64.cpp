// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "base64.hpp"

using namespace Microsoft::Console::VirtualTerminal;

static const wchar_t base64Chars[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const wchar_t padChar = L'=';

#pragma warning(disable : 26446 26447 26482 26485 26493 26494)

// Routine Description:
// - Encode a string using base64. When there are not enough characters
//      for one quantum, paddings are added.
// Arguments:
// - src - String to base64 encode.
// Return Value:
// - the encoded string.
std::wstring Base64::s_Encode(const std::wstring_view src) noexcept
{
    std::wstring dst;
    wchar_t input[3];

    const auto len = (src.size() + 2) / 3 * 4;
    if (len == 0)
    {
        return dst;
    }
    dst.reserve(len);

    auto iter = src.cbegin();
    // Encode each three chars into one quantum (four chars).
    while (iter < src.cend() - 2)
    {
        input[0] = *iter++;
        input[1] = *iter++;
        input[2] = *iter++;
        dst.push_back(base64Chars[input[0] >> 2]);
        dst.push_back(base64Chars[(input[0] & 0x03) << 4 | input[1] >> 4]);
        dst.push_back(base64Chars[(input[1] & 0x0f) << 2 | input[2] >> 6]);
        dst.push_back(base64Chars[(input[2] & 0x3f)]);
    }

    // Here only zero, or one, or two chars are left. We may need to add paddings.
    if (iter < src.cend())
    {
        input[0] = *iter++;
        dst.push_back(base64Chars[input[0] >> 2]);
        if (iter < src.cend()) // Two chars left.
        {
            input[1] = *iter++;
            dst.push_back(base64Chars[(input[0] & 0x03) << 4 | input[1] >> 4]);
            dst.push_back(base64Chars[(input[1] & 0x0f) << 2]);
        }
        else // Only one char left.
        {
            dst.push_back(base64Chars[(input[0] & 0x03) << 4]);
            dst.push_back(padChar);
        }
        dst.push_back(padChar);
    }

    return dst;
}

// Routine Description:
// - Decode a base64 string. This requires the base64 string is properly padded.
//      Otherwise, false will be returned.
// Arguments:
// - src - String to decode.
// - dst - Destination to decode into.
// Return Value:
// - true if decoding successfully, otherwise false.
bool Base64::s_Decode(const std::wstring_view src, std::wstring& dst) noexcept
{
    int state = 0;
    wchar_t tmp;

    const auto len = src.size() / 4 * 3;
    if (len == 0)
    {
        return false;
    }
    dst.reserve(len);

    auto iter = src.cbegin();
    while (iter < src.cend())
    {
        if (s_IsSpace(*iter)) // Skip whitespace anywhere.
        {
            iter++;
            continue;
        }

        if (*iter == padChar)
        {
            break;
        }

        auto pos = wcschr(base64Chars, *iter);
        if (!pos) // A non-base64 character found.
        {
            return false;
        }

        switch (state)
        {
        case 0:
            tmp = (wchar_t)(pos - base64Chars) << 2;
            state = 1;
            break;
        case 1:
            tmp |= (pos - base64Chars) >> 4;
            dst.push_back(tmp);
            tmp = (wchar_t)((pos - base64Chars) & 0x0f) << 4;
            state = 2;
            break;
        case 2:
            tmp |= (pos - base64Chars) >> 2;
            dst.push_back(tmp);
            tmp = (wchar_t)((pos - base64Chars) & 0x03) << 6;
            state = 3;
            break;
        case 3:
            tmp |= pos - base64Chars;
            dst.push_back(tmp);
            state = 0;
            break;
        }

        iter++;
    }

    if (iter < src.cend()) // Padding char is met.
    {
        iter++;
        switch (state)
        {
        // Invalid when state is 0 or 1.
        case 0:
        case 1:
            return false;
        case 2:
            // Skip any number of spaces.
            while (iter < src.cend() && s_IsSpace(*iter))
            {
                iter++;
            }
            // Make sure there is another trailing padding character.
            if (iter == src.cend() || *iter != padChar)
            {
                return false;
            }
            iter++; // Skip the padding character and fallthrough to "single trailing padding character" case.
        case 3:
            while (iter < src.cend())
            {
                if (!s_IsSpace(*iter))
                {
                    return false;
                }
                iter++;
            }
        }
    }
    else if (state != 0) // When no padding, we must be in state 0.
    {
        return false;
    }

    return true;
}

// Routine Description:
// - Check if parameter is a base64 whitespace. Only carriage return or line feed
//      is valid whitespace.
// Arguments:
// - ch - Character to check.
// Return Value:
// - true iff ch is a carriage return or line feed.
constexpr bool Base64::s_IsSpace(const wchar_t ch) noexcept
{
    return ch == L'\r' || ch == L'\n';
}
