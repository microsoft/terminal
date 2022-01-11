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

void DoSrvAddHyperlink(SCREEN_INFORMATION& screenInfo,
                       const std::wstring_view uri,
                       const std::wstring_view params);

void DoSrvEndHyperlink(SCREEN_INFORMATION& screenInfo);

[[nodiscard]] HRESULT DoSrvUpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                                          const SIZE cellSize,
                                          const size_t centeringHint) noexcept;

[[nodiscard]] HRESULT DoSrvSetConsoleOutputCodePage(const unsigned int codepage);

[[nodiscard]] HRESULT DoSrvPrivateScrollRegion(SCREEN_INFORMATION& screenInfo,
                                               const SMALL_RECT scrollRect,
                                               const std::optional<SMALL_RECT> clipRect,
                                               const COORD destinationOrigin,
                                               const bool standardFillAttrs) noexcept;
