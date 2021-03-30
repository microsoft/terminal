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

constexpr til::color COLOR_WHITE{ 0xff, 0xff, 0xff };
constexpr til::color COLOR_BLACK{ 0, 0, 0 };
constexpr COLORREF OPACITY_OPAQUE = 0xff000000;

constexpr auto DEFAULT_FOREGROUND = COLOR_WHITE;
constexpr auto DEFAULT_BACKGROUND = COLOR_BLACK;

constexpr short DEFAULT_HISTORY_SIZE = 9001;

#pragma warning(push)
#pragma warning(disable : 26426)
// TODO GH 2674, don't disable this warning, move to std::wstring_view or something like that.
const std::wstring DEFAULT_FONT_FACE{ L"Cascadia Mono" };
constexpr int DEFAULT_FONT_SIZE = 12;
constexpr uint16_t DEFAULT_FONT_WEIGHT = 400; // normal

constexpr int DEFAULT_ROWS = 30;
constexpr int DEFAULT_COLS = 120;

const std::wstring DEFAULT_PADDING{ L"8, 8, 8, 8" };
const std::wstring DEFAULT_STARTING_DIRECTORY{ L"%USERPROFILE%" };

constexpr auto DEFAULT_CURSOR_COLOR = COLOR_WHITE;
constexpr COLORREF DEFAULT_CURSOR_HEIGHT = 25;

const std::wstring DEFAULT_WORD_DELIMITERS{ L" ./\\()\"'-:,.;<>~!@#$%^&*|+=[]{}~?\u2502" };
#pragma warning(pop)
