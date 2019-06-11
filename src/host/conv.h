/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conv.h

Abstract:
- This module contains the internal structures and definitions used by the conversion area.
- "Conversion area" refers to either the in-line area where the text color changes and suggests options in IME-based languages
  or to the reserved line at the bottom of the screen offering suggestions and the current IME mode.

Author:
- KazuM March 8, 1993

Revision History:
--*/

#pragma once

#include "server.h"

#include "../types/inc/Viewport.hpp"

void WriteConvRegionToScreen(const SCREEN_INFORMATION& ScreenInfo,
                             const Microsoft::Console::Types::Viewport& convRegion);

[[nodiscard]] HRESULT ConsoleImeResizeCompStrView();
[[nodiscard]] HRESULT ConsoleImeResizeCompStrScreenBuffer(const COORD coordNewScreenSize);
