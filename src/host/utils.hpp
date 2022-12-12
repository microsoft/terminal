/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- utils.hpp

Abstract:
- This module contains utility math functions that help perform calculations elsewhere in the console

Author(s):
- Paul Campbell (PaulCam)     2014
- Michael Niksa (MiNiksa)     2014
--*/
#pragma once

#include "conapi.h"
#include "server.h"

#include "../server/ObjectHandle.h"

#define RECT_WIDTH(x) ((x)->right - (x)->left)
#define RECT_HEIGHT(x) ((x)->bottom - (x)->top)

til::CoordType CalcWindowSizeX(const til::inclusive_rect& rect) noexcept;
til::CoordType CalcWindowSizeY(const til::inclusive_rect& rect) noexcept;
til::CoordType CalcCursorYOffsetInPixels(const til::CoordType sFontSizeY, const ULONG ulSize) noexcept;
WORD ConvertStringToDec(_In_ PCWSTR pwchToConvert, _Out_opt_ PCWSTR* const ppwchEnd) noexcept;

std::wstring _LoadString(const UINT id);
static UINT s_LoadStringEx(_In_ HINSTANCE hModule,
                           _In_ UINT wID,
                           _Out_writes_(cchBufferMax) LPWSTR lpBuffer,
                           _In_ UINT cchBufferMax,
                           _In_ WORD wLangId);

class Utils
{
public:
    static int s_CompareCoords(const til::size bufferSize, const til::point first, const til::point second) noexcept;
    static int s_CompareCoords(const til::point coordFirst, const til::point coordSecond) noexcept;

    static til::point s_GetOppositeCorner(const til::inclusive_rect& srRectangle, const til::point coordCorner) noexcept;
};
