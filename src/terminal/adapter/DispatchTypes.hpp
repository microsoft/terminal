// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::VirtualTerminal::DispatchTypes
{
    enum class EraseType : unsigned int
    {
        ToEnd = 0,
        FromBeginning = 1,
        All = 2,
        Scrollback = 3
    };

    // TODO:GH#2916 add support for DoublyUnderlined, Faint(2) to the adapter as well.
    enum GraphicsOptions : unsigned int
    {
        Off = 0,
        BoldBright = 1,
        // The 2 and 5 entries here are for BOTH the extended graphics options,
        // as well as the Faint/Blink options.
        RGBColorOrFaint = 2, // 2 is also Faint, decreased intensity (ISO 6429).
        Italics = 3,
        Underline = 4,
        BlinkOrXterm256Index = 5, // 5 is also Blink (appears as Bold).
        Negative = 7,
        Invisible = 8,
        CrossedOut = 9,
        DoublyUnderlined = 21,
        UnBold = 22,
        NotItalics = 23,
        NoUnderline = 24,
        Steady = 25, // _not_ blink
        Positive = 27, // _not_ inverse
        Visible = 28, // _not_ invisible
        NotCrossedOut = 29,
        ForegroundBlack = 30,
        ForegroundRed = 31,
        ForegroundGreen = 32,
        ForegroundYellow = 33,
        ForegroundBlue = 34,
        ForegroundMagenta = 35,
        ForegroundCyan = 36,
        ForegroundWhite = 37,
        ForegroundExtended = 38,
        ForegroundDefault = 39,
        BackgroundBlack = 40,
        BackgroundRed = 41,
        BackgroundGreen = 42,
        BackgroundYellow = 43,
        BackgroundBlue = 44,
        BackgroundMagenta = 45,
        BackgroundCyan = 46,
        BackgroundWhite = 47,
        BackgroundExtended = 48,
        BackgroundDefault = 49,
        BrightForegroundBlack = 90,
        BrightForegroundRed = 91,
        BrightForegroundGreen = 92,
        BrightForegroundYellow = 93,
        BrightForegroundBlue = 94,
        BrightForegroundMagenta = 95,
        BrightForegroundCyan = 96,
        BrightForegroundWhite = 97,
        BrightBackgroundBlack = 100,
        BrightBackgroundRed = 101,
        BrightBackgroundGreen = 102,
        BrightBackgroundYellow = 103,
        BrightBackgroundBlue = 104,
        BrightBackgroundMagenta = 105,
        BrightBackgroundCyan = 106,
        BrightBackgroundWhite = 107,
    };

    enum class AnsiStatusType : unsigned int
    {
        CPR_CursorPositionReport = 6,
    };

    enum PrivateModeParams : unsigned short
    {
        DECCKM_CursorKeysMode = 1,
        DECCOLM_SetNumberOfColumns = 3,
        DECOM_OriginMode = 6,
        ATT610_StartCursorBlink = 12,
        DECTCEM_TextCursorEnableMode = 25,
        XTERM_EnableDECCOLMSupport = 40,
        VT200_MOUSE_MODE = 1000,
        BUTTTON_EVENT_MOUSE_MODE = 1002,
        ANY_EVENT_MOUSE_MODE = 1003,
        UTF8_EXTENDED_MODE = 1005,
        SGR_EXTENDED_MODE = 1006,
        ALTERNATE_SCROLL = 1007,
        ASB_AlternateScreenBuffer = 1049
    };

    enum VTCharacterSets : wchar_t
    {
        DEC_LineDrawing = L'0',
        USASCII = L'B'
    };

    enum TabClearType : unsigned short
    {
        ClearCurrentColumn = 0,
        ClearAllColumns = 3
    };

    enum WindowManipulationType : unsigned int
    {
        Invalid = 0,
        RefreshWindow = 7,
        ResizeWindowInCharacters = 8,
    };

    enum class CursorStyle : unsigned int
    {
        BlinkingBlock = 0,
        BlinkingBlockDefault = 1,
        SteadyBlock = 2,
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };

    constexpr short s_sDECCOLMSetColumns = 132;
    constexpr short s_sDECCOLMResetColumns = 80;

}
