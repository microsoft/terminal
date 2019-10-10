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
    // TODO:GH#2916 add support for these to the parser as well.
    Underlined = 0x20, // _technically_ different from LVB_UNDERSCORE, see TODO:GH#2915
    DoublyUnderlined = 0x40, // Included for completeness, but not currently supported.
    Faint = 0x80, // Included for completeness, but not currently supported.
};
DEFINE_ENUM_FLAG_OPERATORS(ExtendedAttributes);

WORD FindNearestTableIndex(const COLORREF Color,
                           _In_reads_(cColorTable) const COLORREF* const ColorTable,
                           const WORD cColorTable);

bool FindTableIndex(const COLORREF Color,
                    _In_reads_(cColorTable) const COLORREF* const ColorTable,
                    const WORD cColorTable,
                    _Out_ WORD* const pFoundIndex);

WORD XtermToWindowsIndex(const size_t index) noexcept;
WORD Xterm256ToWindowsIndex(const size_t index) noexcept;
WORD XtermToLegacy(const size_t xtermForeground, const size_t xtermBackground);

COLORREF ForegroundColor(const WORD wLegacyAttrs,
                         _In_reads_(cColorTable) const COLORREF* const ColorTable,
                         const size_t cColorTable);

COLORREF BackgroundColor(const WORD wLegacyAttrs,
                         _In_reads_(cColorTable) const COLORREF* const ColorTable,
                         const size_t cColorTable);

const WORD WINDOWS_RED_ATTR = FOREGROUND_RED;
const WORD WINDOWS_GREEN_ATTR = FOREGROUND_GREEN;
const WORD WINDOWS_BLUE_ATTR = FOREGROUND_BLUE;
const WORD WINDOWS_BRIGHT_ATTR = FOREGROUND_INTENSITY;

const WORD XTERM_RED_ATTR = 0x01;
const WORD XTERM_GREEN_ATTR = 0x02;
const WORD XTERM_BLUE_ATTR = 0x04;
const WORD XTERM_BRIGHT_ATTR = 0x08;

enum class CursorType : unsigned int
{
    Legacy = 0x0, // uses the cursor's height value to range from underscore-like to full box
    VerticalBar = 0x1, // A single vertical line, '|'
    Underscore = 0x2, // a single horizontal underscore, smaller that the min height legacy cursor.
    EmptyBox = 0x3, // Just the outline of a full box
    FullBox = 0x4 // a full box, similar to legacy with height=100%
};

// Valid COLORREFs are of the pattern 0x00bbggrr. -1 works as an invalid color,
//      as the highest byte of a valid color is always 0.
constexpr COLORREF INVALID_COLOR = 0xffffffff;

constexpr WORD COLOR_TABLE_SIZE = 16;
constexpr WORD XTERM_COLOR_TABLE_SIZE = 256;
