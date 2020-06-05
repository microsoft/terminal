/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    conime.h

Abstract:

    This module contains the internal structures and definitions used
    by the console IME.

Author:

    v-HirShi Jul.4.1995

Revision History:

--*/

#pragma once

constexpr unsigned short CONIME_ATTRCOLOR_SIZE = 8;

constexpr BYTE CONIME_CURSOR_RIGHT = 0x10;
constexpr BYTE CONIME_CURSOR_LEFT = 0x20;

[[nodiscard]] HRESULT ImeStartComposition();

[[nodiscard]] HRESULT ImeEndComposition();

[[nodiscard]] HRESULT ImeComposeData(std::wstring_view text,
                                     std::basic_string_view<BYTE> attributes,
                                     std::basic_string_view<WORD> colorArray);

[[nodiscard]] HRESULT ImeClearComposeData();

[[nodiscard]] HRESULT ImeComposeResult(std::wstring_view text);

// Default composition color attributes
#define DEFAULT_COMP_ENTERED                               \
    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | \
     COMMON_LVB_UNDERSCORE)
#define DEFAULT_COMP_ALREADY_CONVERTED                     \
    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | \
     BACKGROUND_BLUE)
#define DEFAULT_COMP_CONVERSION                            \
    (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | \
     COMMON_LVB_UNDERSCORE)
#define DEFAULT_COMP_YET_CONVERTED                         \
    (FOREGROUND_BLUE |                                     \
     BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | \
     COMMON_LVB_UNDERSCORE)
#define DEFAULT_COMP_INPUT_ERROR \
    (FOREGROUND_RED |            \
     COMMON_LVB_UNDERSCORE)
