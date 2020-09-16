// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class VTID
    {
    public:
        template<size_t Length>
        constexpr VTID(const char (&s)[Length]) :
            _value{ _FromString(s) }
        {
        }

        constexpr VTID(const uint64_t value) :
            _value{ value }
        {
        }

        constexpr operator uint64_t() const
        {
            return _value;
        }

        constexpr char operator[](const size_t offset) const
        {
            return SubSequence(offset)._value & 0xFF;
        }

        constexpr VTID SubSequence(const size_t offset) const
        {
            return _value >> (CHAR_BIT * offset);
        }

    private:
        template<size_t Length>
        static constexpr uint64_t _FromString(const char (&s)[Length])
        {
            static_assert(Length - 1 <= sizeof(_value));
            uint64_t value = 0;
            for (auto i = Length - 1; i-- > 0;)
            {
                value = (value << CHAR_BIT) + gsl::at(s, i);
            }
            return value;
        }

        uint64_t _value;
    };

    class VTIDBuilder
    {
    public:
        void Clear() noexcept
        {
            _idAccumulator = 0;
            _idShift = 0;
        }

        void AddIntermediate(const wchar_t intermediateChar) noexcept
        {
            if (_idShift + CHAR_BIT >= sizeof(_idAccumulator) * CHAR_BIT)
            {
                // If there is not enough space in the accumulator to add
                // the intermediate and still have room left for the final,
                // then we reset the accumulator to zero. This will result
                // in an id with all zero intermediates, which shouldn't
                // match anything.
                _idAccumulator = 0;
            }
            else
            {
                // Otherwise we shift the intermediate so as to add it to the
                // accumulator in the next available space, and then increment
                // the shift by 8 bits in preparation for the next character.
                _idAccumulator += (static_cast<uint64_t>(intermediateChar) << _idShift);
                _idShift += CHAR_BIT;
            }
        }

        VTID Finalize(const wchar_t finalChar) noexcept
        {
            return _idAccumulator + (static_cast<uint64_t>(finalChar) << _idShift);
        }

    private:
        uint64_t _idAccumulator = 0;
        size_t _idShift = 0;
    };
}

namespace Microsoft::Console::VirtualTerminal::DispatchTypes
{
    enum class EraseType : unsigned int
    {
        ToEnd = 0,
        FromBeginning = 1,
        All = 2,
        Scrollback = 3
    };

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
        NotBoldOrFaint = 22,
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
        Overline = 53,
        NoOverline = 55,
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
        OS_OperatingStatus = 5,
        CPR_CursorPositionReport = 6,
    };

    enum PrivateModeParams : unsigned short
    {
        DECCKM_CursorKeysMode = 1,
        DECANM_AnsiMode = 2,
        DECCOLM_SetNumberOfColumns = 3,
        DECSCNM_ScreenMode = 5,
        DECOM_OriginMode = 6,
        DECAWM_AutoWrapMode = 7,
        ATT610_StartCursorBlink = 12,
        DECTCEM_TextCursorEnableMode = 25,
        XTERM_EnableDECCOLMSupport = 40,
        VT200_MOUSE_MODE = 1000,
        BUTTON_EVENT_MOUSE_MODE = 1002,
        ANY_EVENT_MOUSE_MODE = 1003,
        UTF8_EXTENDED_MODE = 1005,
        SGR_EXTENDED_MODE = 1006,
        ALTERNATE_SCROLL = 1007,
        ASB_AlternateScreenBuffer = 1049,
        W32IM_Win32InputMode = 9001
    };

    enum CharacterSets : uint64_t
    {
        DecSpecialGraphics = VTID("0"),
        ASCII = VTID("B")
    };

    enum CodingSystem : uint64_t
    {
        ISO2022 = VTID("@"),
        UTF8 = VTID("G")
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
        UserDefault = 0, // Implemented as "restore cursor to user default".
        BlinkingBlock = 1,
        SteadyBlock = 2,
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };

    enum class LineFeedType : unsigned int
    {
        WithReturn,
        WithoutReturn,
        DependsOnMode
    };

    constexpr short s_sDECCOLMSetColumns = 132;
    constexpr short s_sDECCOLMResetColumns = 80;

}
