// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "inc/Utf16Parser.hpp"
#include "unicode.hpp"

// Routine Description:
// - Finds the next single collection for the codepoint out of the given UTF-16 string information.
// - In simpler terms, it will group UTF-16 surrogate pairs into a single unit or give you a valid single-item UTF-16 character.
// - Does not validate UTF-16 input beyond proper leading/trailing character sequences.
// Arguments:
// - wstr - The UTF-16 string to parse.
// Return Value:
// - A view into the string given of just the next codepoint unit.
std::wstring_view Utf16Parser::ParseNext(std::wstring_view wstr) noexcept
{
    for (size_t pos = 0; pos < wstr.size(); ++pos)
    {
        const auto wch = wstr.at(pos);

        // If it's a lead and followed directly by a trail, then return the pair.
        // If it's not followed directly by the trail, go around again and seek forward.
        if (IsLeadingSurrogate(wch))
        {
            // Try to find the next item... if it isn't there, we'll go around again.
            const auto posNext = pos + 1;
            if (posNext < wstr.size())
            {
                // If we found it and it's trailing, return the pair.
                const auto wchNext = wstr.at(posNext);
                if (IsTrailingSurrogate(wchNext))
                {
                    return wstr.substr(pos, 2);
                }
            }
            // If we missed either if in any way, we'll fall through and go around again searching for more.
        }
        // If it's just a trail at this point, go around again and seek forward.
        else if (IsTrailingSurrogate(wch))
        {
            continue;
        }
        // If it's neither lead nor trail, then it's < U+10000 and it can be returned as a single wchar_t point.
        else
        {
            return wstr.substr(pos, 1);
        }
    }

    // If we get all the way through and there's nothing valid, then this is just a replacement character as it was broken/garbage.
    return std::wstring_view{ &UNICODE_REPLACEMENT, 1 };
}

// Routine Description:
// - formats a utf16 encoded wstring and splits the codepoints into individual collections.
// - will drop badly formatted leading/trailing char sequences.
// - does not validate utf16 input beyond proper leading/trailing char sequences.
// Arguments:
// - wstr - the string to parse
// Return Value:
// - a vector of utf16 codepoints. glyphs that require surrogate pairs will be grouped
// together in a vector and codepoints that use only one wchar will be in a vector by themselves.
std::vector<std::vector<wchar_t>> Utf16Parser::Parse(std::wstring_view wstr)
{
    std::vector<std::vector<wchar_t>> result;
    std::vector<wchar_t> sequence;
    for (const auto wch : wstr)
    {
        if (IsLeadingSurrogate(wch))
        {
            sequence.clear();
            sequence.push_back(wch);
        }
        else if (IsTrailingSurrogate(wch))
        {
            if (!sequence.empty())
            {
                sequence.push_back(wch);
                result.push_back(sequence);
                sequence.clear();
            }
        }
        else
        {
            result.push_back({ wch });
        }
    }
    return result;
}
