/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- getset.h

Abstract:
- This file implements the NT console server console state API.

Author:
- Therese Stowell (ThereseS) 5-Dec-1990

Revision History:
--*/

#pragma once
#include "../inc/conattrs.hpp"
class SCREEN_INFORMATION;

[[nodiscard]] NTSTATUS DoSrvPrivateSetCursorKeysMode(_In_ bool fApplicationMode);
[[nodiscard]] NTSTATUS DoSrvPrivateSetKeypadMode(_In_ bool fApplicationMode);
void DoSrvPrivateEnableWin32InputMode(const bool win32InputMode);

[[nodiscard]] NTSTATUS DoSrvPrivateSetScreenMode(const bool reverseMode);
[[nodiscard]] NTSTATUS DoSrvPrivateSetAutoWrapMode(const bool wrapAtEOL);

void DoSrvPrivateShowCursor(SCREEN_INFORMATION& screenInfo, const bool show) noexcept;
void DoSrvPrivateAllowCursorBlinking(SCREEN_INFORMATION& screenInfo, const bool fEnable);

[[nodiscard]] NTSTATUS DoSrvPrivateSetScrollingRegion(SCREEN_INFORMATION& screenInfo, const SMALL_RECT& scrollMargins);
[[nodiscard]] NTSTATUS DoSrvPrivateLineFeed(SCREEN_INFORMATION& screenInfo, const bool withReturn);
[[nodiscard]] NTSTATUS DoSrvPrivateReverseLineFeed(SCREEN_INFORMATION& screenInfo);

[[nodiscard]] NTSTATUS DoSrvPrivateUseAlternateScreenBuffer(SCREEN_INFORMATION& screenInfo);
void DoSrvPrivateUseMainScreenBuffer(SCREEN_INFORMATION& screenInfo);

void DoSrvPrivateEnableVT200MouseMode(const bool fEnable);
void DoSrvPrivateEnableUTF8ExtendedMouseMode(const bool fEnable);
void DoSrvPrivateEnableSGRExtendedMouseMode(const bool fEnable);
void DoSrvPrivateEnableButtonEventMouseMode(const bool fEnable);
void DoSrvPrivateEnableAnyEventMouseMode(const bool fEnable);
void DoSrvPrivateEnableAlternateScroll(const bool fEnable);

[[nodiscard]] HRESULT DoSrvPrivateEraseAll(SCREEN_INFORMATION& screenInfo);

void DoSrvSetCursorStyle(SCREEN_INFORMATION& screenInfo,
                         const CursorType cursorType);
void DoSrvSetCursorColor(SCREEN_INFORMATION& screenInfo,
                         const COLORREF cursorColor);

void DoSrvPrivateRefreshWindow(const SCREEN_INFORMATION& screenInfo);

[[nodiscard]] HRESULT DoSrvSetConsoleOutputCodePage(const unsigned int codepage);
void DoSrvGetConsoleOutputCodePage(unsigned int& codepage);

[[nodiscard]] NTSTATUS DoSrvPrivateSuppressResizeRepaint();

void DoSrvIsConsolePty(bool& isPty);

void DoSrvPrivateDeleteLines(const size_t count);
void DoSrvPrivateInsertLines(const size_t count);

void DoSrvPrivateMoveToBottom(SCREEN_INFORMATION& screenInfo);

[[nodiscard]] HRESULT DoSrvPrivateGetColorTableEntry(const size_t index, COLORREF& value) noexcept;
[[nodiscard]] HRESULT DoSrvPrivateSetColorTableEntry(const size_t index, const COLORREF value) noexcept;

[[nodiscard]] HRESULT DoSrvPrivateSetDefaultForegroundColor(const COLORREF value) noexcept;

[[nodiscard]] HRESULT DoSrvPrivateSetDefaultBackgroundColor(const COLORREF value) noexcept;

[[nodiscard]] HRESULT DoSrvPrivateFillRegion(SCREEN_INFORMATION& screenInfo,
                                             const COORD startPosition,
                                             const size_t fillLength,
                                             const wchar_t fillChar,
                                             const bool standardFillAttrs) noexcept;

[[nodiscard]] HRESULT DoSrvPrivateScrollRegion(SCREEN_INFORMATION& screenInfo,
                                               const SMALL_RECT scrollRect,
                                               const std::optional<SMALL_RECT> clipRect,
                                               const COORD destinationOrigin,
                                               const bool standardFillAttrs) noexcept;
