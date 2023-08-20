/*++
Copyright (c) Microsoft Corporation

Module Name:
- CodepointWidthDetector.hpp

Abstract:
- Object used to measure the width of a codepoint when it's rendered

Author:
- Austin Diviness (AustDi) 18-May-2018
--*/

#pragma once

#include "convert.hpp"

// use to measure the width of a codepoint
class CodepointWidthDetector final
{
public:
    CodepointWidth GetWidth(const std::wstring_view& glyph) noexcept;
    bool IsWide(const std::wstring_view& glyph) noexcept;
    void SetFallbackMethod(std::function<bool(const std::wstring_view&)> pfnFallback) noexcept;
    void NotifyFontChanged() noexcept;

#ifdef UNIT_TESTING
    friend class CodepointWidthDetectorTests;
#endif

private:
    uint8_t _lookupGlyphWidth(char32_t codepoint, const std::wstring_view& glyph) noexcept;
    uint8_t _checkFallbackViaCache(char32_t codepoint, const std::wstring_view& glyph) noexcept;

    std::unordered_map<char32_t, uint8_t> _fallbackCache;
    std::function<bool(const std::wstring_view&)> _pfnFallbackMethod;
};
