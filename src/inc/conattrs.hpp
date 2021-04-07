/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once

#define FG_ATTRS (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)
#define BG_ATTRS (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY)
#define META_ATTRS (COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE | COMMON_LVB_GRID_HORIZONTAL | COMMON_LVB_GRID_LVERTICAL | COMMON_LVB_GRID_RVERTICAL | COMMON_LVB_REVERSE_VIDEO | COMMON_LVB_UNDERSCORE)

enum class ExtendedAttributes : BYTE
{
    Normal = 0x00,
    Bold = 0x01,
    Italics = 0x02,
    Blinking = 0x04,
    Invisible = 0x08,
    CrossedOut = 0x10,
    Underlined = 0x20,
    DoublyUnderlined = 0x40,
    Faint = 0x80,
};
DEFINE_ENUM_FLAG_OPERATORS(ExtendedAttributes);

WORD XtermToWindowsIndex(const size_t index) noexcept;
WORD Xterm256ToWindowsIndex(const size_t index) noexcept;
WORD XtermToLegacy(const size_t xtermForeground, const size_t xtermBackground);

constexpr WORD WINDOWS_RED_ATTR = FOREGROUND_RED;
constexpr WORD WINDOWS_GREEN_ATTR = FOREGROUND_GREEN;
constexpr WORD WINDOWS_BLUE_ATTR = FOREGROUND_BLUE;
constexpr WORD WINDOWS_BRIGHT_ATTR = FOREGROUND_INTENSITY;

constexpr WORD XTERM_RED_ATTR = 0x01;
constexpr WORD XTERM_GREEN_ATTR = 0x02;
constexpr WORD XTERM_BLUE_ATTR = 0x04;
constexpr WORD XTERM_BRIGHT_ATTR = 0x08;

enum class CursorType : unsigned int
{
    Legacy = 0x0, // uses the cursor's height value to range from underscore-like to full box
    VerticalBar = 0x1, // A single vertical line, '|'
    Underscore = 0x2, // a single horizontal underscore, smaller that the min height legacy cursor.
    EmptyBox = 0x3, // Just the outline of a full box
    FullBox = 0x4, // a full box, similar to legacy with height=100%
    DoubleUnderscore = 0x5 // a double horizontal underscore
};

// Valid COLORREFs are of the pattern 0x00bbggrr. -1 works as an invalid color,
//      as the highest byte of a valid color is always 0.
constexpr COLORREF INVALID_COLOR = 0xffffffff;

constexpr WORD COLOR_TABLE_SIZE = 16;
constexpr WORD XTERM_COLOR_TABLE_SIZE = 256;
