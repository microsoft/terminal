/*++
Copyright (c) Microsoft Corporation

Module Name:
- Utf16Parser.hpp

Abstract:
- Parser for grouping together utf16 codepoints from a string of utf16 encoded text

Author(s):
- Austin Diviness (AustDi) 25-Apr-2018

--*/

#pragma once

#include <vector>
#include <optional>
#include <bitset>

class Utf16Parser final
{
private:
    static constexpr unsigned short IndicatorBitCount = 6;
    static constexpr unsigned short WcharShiftAmount = sizeof(wchar_t) * 8 - IndicatorBitCount;
    static constexpr std::bitset<IndicatorBitCount> LeadingSurrogateMask = { 54 }; // 110 110 indicates a leading surrogate
    static constexpr std::bitset<IndicatorBitCount> TrailingSurrogateMask = { 55 }; // 110 111 indicates a trailing surrogate

public:
    static std::vector<std::vector<wchar_t>> Parse(std::wstring_view wstr);
    static std::wstring_view ParseNext(std::wstring_view wstr) noexcept;

    // Routine Description:
    // - checks if wchar is a utf16 leading surrogate
    // Arguments:
    // - wch - the wchar to check
    // Return Value:
    // - true if wch is a leading surrogate, false otherwise
    static inline bool IsLeadingSurrogate(const wchar_t wch) noexcept
    {
        const wchar_t bits = wch >> WcharShiftAmount;
        const std::bitset<IndicatorBitCount> possBits = { bits };
        return (possBits ^ LeadingSurrogateMask).none();
    }

    // Routine Description:
    // - checks if wchar is a utf16 trailing surrogate
    // Arguments:
    // - wch - the wchar to check
    // Return Value:
    // - true if wch is a trailing surrogate, false otherwise
    static inline bool IsTrailingSurrogate(const wchar_t wch) noexcept
    {
        const wchar_t bits = wch >> WcharShiftAmount;
        const std::bitset<IndicatorBitCount> possBits = { bits };
        return (possBits ^ TrailingSurrogateMask).none();
    }
};
