/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- search.h

Abstract:
- This module is used for searching through the screen for a substring

Author(s):
- Michael Niksa (MiNiksa) 20-Apr-2018

Revision History:
- From components of find.c by Jerry Shea (jerrysh) 1-May-1997
--*/

#pragma once

#include "TextAttribute.hpp"
#include "textBuffer.hpp"
#include "../renderer/inc/IRenderData.hpp"

// This used to be in find.h.
#define SEARCH_STRING_LENGTH (80)

class Search final
{
public:
    enum class Direction
    {
        Forward,
        Backward
    };

    enum class Sensitivity
    {
        CaseInsensitive,
        CaseSensitive
    };

    Search(Microsoft::Console::Render::IRenderData& renderData,
           const std::wstring_view str,
           const Direction dir,
           const Sensitivity sensitivity);

    Search(Microsoft::Console::Render::IRenderData& renderData,
           const std::wstring_view str,
           const Direction dir,
           const Sensitivity sensitivity,
           const til::point anchor);

    bool FindNext();
    void Select() const;
    void Color(const TextAttribute attr) const;

    std::pair<til::point, til::point> GetFoundLocation() const noexcept;

private:
    wchar_t _ApplySensitivity(const wchar_t wch) const noexcept;
    bool _FindNeedleInHaystackAt(const til::point pos, til::point& start, til::point& end) const;
    bool _CompareChars(const std::wstring_view one, const std::wstring_view two) const noexcept;
    void _UpdateNextPosition();

    void _IncrementCoord(til::point& coord) const noexcept;
    void _DecrementCoord(til::point& coord) const noexcept;

    static til::point s_GetInitialAnchor(const Microsoft::Console::Render::IRenderData& renderData, const Direction dir);

    static std::vector<std::wstring> s_CreateNeedleFromString(const std::wstring_view wstr);

    bool _reachedEnd = false;
    til::point _coordNext;
    til::point _coordSelStart;
    til::point _coordSelEnd;

    const til::point _coordAnchor;
    const std::vector<std::wstring> _needle;
    const Direction _direction;
    const Sensitivity _sensitivity;
    Microsoft::Console::Render::IRenderData& _renderData;

#ifdef UNIT_TESTING
    friend class SearchTests;
#endif
};
