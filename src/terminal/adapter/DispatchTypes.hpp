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

    class VTParameter
    {
    public:
        constexpr VTParameter() noexcept :
            _value{ -1 }
        {
        }

        constexpr VTParameter(const size_t rhs) noexcept :
            _value{ gsl::narrow_cast<decltype(_value)>(rhs) }
        {
        }

        constexpr bool has_value() const noexcept
        {
            // A negative value indicates that the parameter was omitted.
            return _value >= 0;
        }

        constexpr size_t value() const noexcept
        {
            return _value;
        }

        constexpr size_t value_or(size_t defaultValue) const noexcept
        {
            return has_value() ? _value : defaultValue;
        }

        template<typename T, std::enable_if_t<sizeof(T) == sizeof(size_t), int> = 0>
        constexpr operator T() const noexcept
        {
            // For most selective parameters, omitted values will default to 0.
            return static_cast<T>(value_or(0));
        }

        constexpr operator size_t() const noexcept
        {
            // For numeric parameters, both 0 and omitted values will default to 1.
            return has_value() && _value != 0 ? _value : 1;
        }

    private:
        std::make_signed<size_t>::type _value;
    };

    class VTParameters
    {
    public:
        constexpr VTParameters() noexcept
        {
        }

        constexpr VTParameters(const VTParameter* ptr, const size_t count) noexcept :
            _values{ ptr, count }
        {
        }

        constexpr VTParameter at(const size_t index) const noexcept
        {
            // If the index is out of range, we return a parameter with no value.
            return index < _values.size() ? _values[index] : VTParameter{};
        }

        constexpr bool empty() const noexcept
        {
            return _values.empty();
        }

        constexpr size_t size() const noexcept
        {
            // We always return a size of at least 1, since an empty parameter
            // list is the equivalent of a single "default" parameter.
            return std::max<size_t>(_values.size(), 1);
        }

        VTParameters subspan(const size_t offset) const noexcept
        {
            const auto subValues = _values.subspan(offset);
            return { subValues.data(), subValues.size() };
        }

        template<typename T>
        bool for_each(const T&& predicate) const
        {
            // We always return at least 1 value here, since an empty parameter
            // list is the equivalent of a single "default" parameter.
            auto success = predicate(at(0));
            for (auto i = 1u; i < _values.size(); i++)
            {
                success = predicate(_values[i]) && success;
            }
            return success;
        }

    private:
        gsl::span<const VTParameter> _values;
    };

    // FlaggedEnumValue is a convenience class that produces enum values (of a specified size)
    // with a flag embedded for differentiating different value categories in the same enum.
    //
    // It is intended to be used via type alias:
    // using FirstFlagType  = FlaggedEnumValue<uint8_t, 0x10>;
    // using SecondFlagType = FlaggedEnumValue<uint8_t, 0x20>;
    // enum EnumeratorOfThings : uint8_t {
    //   ThingOfFirstType = FirstFlagType(1),
    //   ThingOfSecondType = SecondFlagType(1)
    // };
    //
    // It will produce an error if the provided flag value sets multiple bits.
    template<typename T, T Flag>
    class FlaggedEnumValue
    {
        template<T Value>
        struct ZeroOrOneBitChecker
        {
            static_assert(Value == 0 || (((Value - 1) & Value) == 0), "zero or one flags expected");
            static constexpr T value = Value;
        };

    public:
        static constexpr T mask{ ZeroOrOneBitChecker<Flag>::value };
        constexpr FlaggedEnumValue(const T value) :
            _value{ value }
        {
        }

        constexpr FlaggedEnumValue(const VTParameter& value) :
            _value{ value }
        {
        }

        template<typename U, std::enable_if_t<sizeof(U) == sizeof(size_t), int> = 0>
        constexpr operator U() const noexcept
        {
            return static_cast<U>(_value | mask);
        }

        constexpr operator T() const noexcept
        {
            return _value | mask;
        }

    private:
        T _value;
    };
}

namespace Microsoft::Console::VirtualTerminal::DispatchTypes
{
    enum class EraseType : size_t
    {
        ToEnd = 0,
        FromBeginning = 1,
        All = 2,
        Scrollback = 3
    };

    enum class TaskbarState : size_t
    {
        Clear = 0,
        Set = 1,
        Error = 2,
        Indeterminate = 3,
        Paused = 4
    };

    enum GraphicsOptions : size_t
    {
        Off = 0,
        BoldBright = 1,
        // The 2 and 5 entries here are for BOTH the extended graphics options,
        // as well as the Faint/Blink options.
        RGBColorOrFaint = 2, // 2 is also Faint, decreased intensity (ISO 6429).
        Italics = 3,
        Underline = 4,
        BlinkOrXterm256Index = 5, // 5 is also Blink (appears as Bold).
        RapidBlink = 6,
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

    // Many of these correspond directly to SGR parameters (the GraphicsOptions enum), but
    // these are distinct (notably 10 and 11, which as SGR parameters would select fonts,
    // are used here to indicate that the foreground/background colors should be saved).
    // From xterm's ctlseqs doc for XTPUSHSGR:
    //
    //      Ps = 1    =>  Bold.
    //      Ps = 2    =>  Faint.
    //      Ps = 3    =>  Italicized.
    //      Ps = 4    =>  Underlined.
    //      Ps = 5    =>  Blink.
    //      Ps = 7    =>  Inverse.
    //      Ps = 8    =>  Invisible.
    //      Ps = 9    =>  Crossed-out characters.
    //      Ps = 2 1  =>  Doubly-underlined.
    //      Ps = 3 0  =>  Foreground color.
    //      Ps = 3 1  =>  Background color.
    //
    enum class SgrSaveRestoreStackOptions : size_t
    {
        All = 0,
        Boldness = 1,
        Faintness = 2,
        Italics = 3,
        Underline = 4,
        Blink = 5,
        Negative = 7,
        Invisible = 8,
        CrossedOut = 9,
        DoublyUnderlined = 21,
        SaveForegroundColor = 30,
        SaveBackgroundColor = 31,
        Max = SaveBackgroundColor
    };

    enum class AnsiStatusType : size_t
    {
        OS_OperatingStatus = 5,
        CPR_CursorPositionReport = 6,
    };

    using ANSIStandardMode = FlaggedEnumValue<size_t, 0x00000000>;
    using DECPrivateMode = FlaggedEnumValue<size_t, 0x01000000>;

    enum ModeParams : size_t
    {
        DECCKM_CursorKeysMode = DECPrivateMode(1),
        DECANM_AnsiMode = DECPrivateMode(2),
        DECCOLM_SetNumberOfColumns = DECPrivateMode(3),
        DECSCNM_ScreenMode = DECPrivateMode(5),
        DECOM_OriginMode = DECPrivateMode(6),
        DECAWM_AutoWrapMode = DECPrivateMode(7),
        ATT610_StartCursorBlink = DECPrivateMode(12),
        DECTCEM_TextCursorEnableMode = DECPrivateMode(25),
        XTERM_EnableDECCOLMSupport = DECPrivateMode(40),
        VT200_MOUSE_MODE = DECPrivateMode(1000),
        BUTTON_EVENT_MOUSE_MODE = DECPrivateMode(1002),
        ANY_EVENT_MOUSE_MODE = DECPrivateMode(1003),
        UTF8_EXTENDED_MODE = DECPrivateMode(1005),
        SGR_EXTENDED_MODE = DECPrivateMode(1006),
        ALTERNATE_SCROLL = DECPrivateMode(1007),
        ASB_AlternateScreenBuffer = DECPrivateMode(1049),
        XTERM_BracketedPasteMode = DECPrivateMode(2004),
        W32IM_Win32InputMode = DECPrivateMode(9001),
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

    enum TabClearType : size_t
    {
        ClearCurrentColumn = 0,
        ClearAllColumns = 3
    };

    enum WindowManipulationType : size_t
    {
        Invalid = 0,
        RefreshWindow = 7,
        ResizeWindowInCharacters = 8,
    };

    enum class CursorStyle : size_t
    {
        UserDefault = 0, // Implemented as "restore cursor to user default".
        BlinkingBlock = 1,
        SteadyBlock = 2,
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };

    enum class ReportingPermission : size_t
    {
        Unsolicited = 0,
        Solicited = 1
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
