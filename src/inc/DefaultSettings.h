/*++
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:
- DefaultSettings.h

Abstract:
- A header with a bunch of default values used for settings, especially for
        terminal settings used by Cascadia
Author(s):
- Mike Griese - March 2019

--*/
#pragma once

constexpr COLORREF COLOR_WHITE = 0x00ffffff;
constexpr COLORREF COLOR_BLACK = 0x00000000;
constexpr COLORREF OPACITY_OPAQUE = 0xff000000;

constexpr COLORREF DEFAULT_FOREGROUND = COLOR_WHITE;
constexpr COLORREF DEFAULT_FOREGROUND_WITH_ALPHA = OPACITY_OPAQUE | DEFAULT_FOREGROUND;
constexpr COLORREF DEFAULT_BACKGROUND = COLOR_BLACK;
constexpr COLORREF DEFAULT_BACKGROUND_WITH_ALPHA = OPACITY_OPAQUE | DEFAULT_BACKGROUND;

constexpr COLORREF POWERSHELL_BLUE = RGB(1, 36, 86);

constexpr short DEFAULT_HISTORY_SIZE = 9001;

#pragma warning(push)
#pragma warning(disable : 26426)
// TODO GH 2674, don't disable this warning, move to std::wstring_view or something like that.
const std::wstring DEFAULT_FONT_FACE{ L"Consolas" };
constexpr int DEFAULT_FONT_SIZE = 12;

constexpr int DEFAULT_ROWS = 30;
constexpr int DEFAULT_COLS = 120;

const std::wstring DEFAULT_PADDING{ L"8, 8, 8, 8" };
const std::wstring DEFAULT_STARTING_DIRECTORY{ L"%USERPROFILE%" };

constexpr COLORREF DEFAULT_CURSOR_COLOR = COLOR_WHITE;
constexpr COLORREF DEFAULT_CURSOR_HEIGHT = 25;

const std::wstring DEFAULT_WORD_DELIMITERS{ L" ./\\()\"'-:,.;<>~!@#$%^&*|+=[]{}~?\u2502" };
#pragma warning(pop)
