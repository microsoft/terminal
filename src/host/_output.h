/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- _output.h

Abstract:
- These methods provide processing of the text buffer into the final screen rendering state
- For all languages (CJK and Western)
- Most GDI work is processed in these classes

Author(s):
- KazuM Jun.11.1997

Revision History:
- Remove FE/Non-FE separation in preparation for refactoring. (MiNiksa, 2014)
--*/

#pragma once

#include "screenInfo.hpp"
#include "../types/inc/viewport.hpp"

void WriteToScreen(SCREEN_INFORMATION& screenInfo, const Microsoft::Console::Types::Viewport& region);
