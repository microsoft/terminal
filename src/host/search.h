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

    Search(const SCREEN_INFORMATION& ScreenInfo,
           const std::wstring& str,
           const Direction dir,
           const Sensitivity sensitivity);

    Search(const SCREEN_INFORMATION& ScreenInfo,
           const std::wstring& str,
           const Direction dir,
           const Sensitivity sensitivity,
           const COORD anchor);

    bool FindNext();
    void Select() const;
    void Color(const TextAttribute attr) const;

    std::pair<COORD, COORD> GetFoundLocation() const noexcept;

private:
    wchar_t _ApplySensitivity(const wchar_t wch) const;
    bool Search::_FindNeedleInHaystackAt(const COORD pos, COORD& start, COORD& end) const;
    bool _CompareChars(const std::wstring_view one, const std::wstring_view two) const;
    void _UpdateNextPosition();

    void _IncrementCoord(COORD& coord) const;
    void _DecrementCoord(COORD& coord) const;

    static COORD s_GetInitialAnchor(const SCREEN_INFORMATION& screenInfo, const Direction dir);
    static std::vector<std::vector<wchar_t>> s_CreateNeedleFromString(const std::wstring& wstr);

    bool _reachedEnd = false;
    COORD _coordNext = { 0 };
    COORD _coordSelStart = { 0 };
    COORD _coordSelEnd = { 0 };

    const COORD _coordAnchor;
    const std::vector<std::vector<wchar_t>> _needle;
    const Direction _direction;
    const Sensitivity _sensitivity;
    const SCREEN_INFORMATION& _screenInfo;

#ifdef UNIT_TESTING
    friend class SearchTests;
#endif
};
