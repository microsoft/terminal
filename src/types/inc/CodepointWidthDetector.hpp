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
#include <functional>

static_assert(sizeof(unsigned int) == sizeof(wchar_t) * 2,
              "UnicodeRange expects to be able to store a unicode codepoint in an unsigned int");

// use to measure the width of a codepoint
class CodepointWidthDetector final
{
public:
    CodepointWidthDetector() noexcept;
    CodepointWidthDetector(const CodepointWidthDetector&) = delete;
    CodepointWidthDetector(CodepointWidthDetector&&) = delete;
    ~CodepointWidthDetector() = default;
    CodepointWidthDetector& operator=(const CodepointWidthDetector&) = delete;
    CodepointWidthDetector& operator=(CodepointWidthDetector&&) = delete;

    CodepointWidth GetWidth(const std::wstring_view glyph) const;
    bool IsWide(const std::wstring_view glyph) const;
    bool IsWide(const wchar_t wch) const noexcept;
    void SetFallbackMethod(std::function<bool(const std::wstring_view)> pfnFallback);
    void NotifyFontChanged() const noexcept;

#ifdef UNIT_TESTING
    friend class CodepointWidthDetectorTests;
#endif

private:
    bool _lookupIsWide(const std::wstring_view glyph) const noexcept;
    bool _checkFallbackViaCache(const std::wstring_view glyph) const;
    static unsigned int _extractCodepoint(const std::wstring_view glyph) noexcept;

    mutable std::map<std::wstring, bool> _fallbackCache;
    std::function<bool(std::wstring_view)> _pfnFallbackMethod;
};
