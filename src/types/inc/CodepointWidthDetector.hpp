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

static_assert(sizeof(unsigned int) == sizeof(wchar_t) * 2,
              "UnicodeRange expects to be able to store a unicode codepoint in an unsigned int");

// use to measure the width of a codepoint
class CodepointWidthDetector final
{
protected:
    // used to store range data in CodepointWidthDetector's internal map
    class UnicodeRange final
    {
    public:
        UnicodeRange(const unsigned int lowerBound,
                     const unsigned int upperBound) :
            _lowerBound{ lowerBound },
            _upperBound{ upperBound },
            _isBounds{ true }
        {
        }

        UnicodeRange(const unsigned int searchTerm) :
            _lowerBound{ searchTerm },
            _upperBound{ searchTerm },
            _isBounds{ false }
        {
        }

        bool IsBounds() const noexcept
        {
            return _isBounds;
        }

        unsigned int LowerBound() const
        {
            FAIL_FAST_IF(!_isBounds);
            return _lowerBound;
        }

        unsigned int UpperBound() const
        {
            FAIL_FAST_IF(!_isBounds);
            return _upperBound;
        }

        unsigned int SearchTerm() const
        {
            FAIL_FAST_IF(_isBounds);
            return _lowerBound;
        }

    private:
        unsigned int _lowerBound;
        unsigned int _upperBound;
        bool _isBounds;
    };

    // used for comparing if we've found the range that a searching UnicodeRange falls into
    struct UnicodeRangeCompare final
    {
        bool operator()(const UnicodeRange& a, const UnicodeRange& b) const
        {
            if (!a.IsBounds() && b.IsBounds())
            {
                return a.SearchTerm() < b.LowerBound();
            }
            else if (a.IsBounds() && !b.IsBounds())
            {
                return a.UpperBound() < b.SearchTerm();
            }
            else if (a.IsBounds() && b.IsBounds())
            {
                return a.LowerBound() < b.LowerBound();
            }
            else
            {
                return a.SearchTerm() < b.SearchTerm();
            }
        }
    };

public:
    CodepointWidthDetector() = default;
    CodepointWidthDetector(const CodepointWidthDetector&) = delete;
    CodepointWidthDetector(CodepointWidthDetector&&) = delete;
    ~CodepointWidthDetector() = default;
    CodepointWidthDetector& operator=(const CodepointWidthDetector&) = delete;

    CodepointWidth GetWidth(const std::wstring_view glyph) const noexcept;
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
    unsigned int _extractCodepoint(const std::wstring_view glyph) const noexcept;
    void _populateUnicodeSearchMap();

    mutable std::map<std::wstring, bool> _fallbackCache;
    std::map<UnicodeRange, CodepointWidth, UnicodeRangeCompare> _map;
    std::function<bool(std::wstring_view)> _pfnFallbackMethod;
    bool _hasFallback = false;
};
