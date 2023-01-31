// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    using VTInt = int32_t;

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

        constexpr VTParameter(const VTInt rhs) noexcept :
            _value{ rhs }
        {
        }

        constexpr bool has_value() const noexcept
        {
            // A negative value indicates that the parameter was omitted.
            return _value >= 0;
        }

        constexpr VTInt value() const noexcept
        {
            return _value;
        }

        constexpr VTInt value_or(VTInt defaultValue) const noexcept
        {
            // A negative value indicates that the parameter was omitted.
            return _value < 0 ? defaultValue : _value;
        }

        template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
        constexpr operator T() const noexcept
        {
            // For most selective parameters, omitted values will default to 0.
            return static_cast<T>(value_or(0));
        }

        constexpr operator VTInt() const noexcept
        {
            // For numeric parameters, both 0 and omitted values will default to 1.
            // The parameter is omitted if _value is less than 0.
            return _value <= 0 ? 1 : _value;
        }

    private:
        VTInt _value;
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
            return index < _values.size() ? til::at(_values, index) : defaultParameter;
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
            auto values = _values;

            // We always return at least 1 value here, since an empty parameter
            // list is the equivalent of a single "default" parameter.
            if (values.empty())
            {
                values = defaultParameters;
            }

            auto success = true;
            for (const auto& v : values)
            {
                success = predicate(v) && success;
            }
            return success;
        }

    private:
        static constexpr VTParameter defaultParameter;
        static constexpr std::span defaultParameters{ &defaultParameter, 1 };

        std::span<const VTParameter> _values;
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
    template<VTInt Flag>
    class FlaggedEnumValue
    {
    public:
        static constexpr VTInt mask{ Flag };

        constexpr FlaggedEnumValue(const VTInt value) :
            _value{ value }
        {
        }

        constexpr FlaggedEnumValue(const VTParameter& value) :
            _value{ value }
        {
        }

        template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
        constexpr operator T() const noexcept
        {
            return static_cast<T>(_value | mask);
        }

        constexpr operator VTInt() const noexcept
        {
            return _value | mask;
        }

    private:
        VTInt _value;
    };
}

namespace Microsoft::Console::VirtualTerminal::DispatchTypes
{
    enum class ColorItem : VTInt
    {
        NormalText = 1,
        WindowFrame = 2,
    };

    enum class ColorModel : VTInt
    {
        HLS = 1,
        RGB = 2,
    };

    enum class EraseType : VTInt
    {
        ToEnd = 0,
        FromBeginning = 1,
        All = 2,
        Scrollback = 3
    };

    enum class ChangeExtent : VTInt
    {
        Default = 0,
        Stream = 1,
        Rectangle = 2
    };

    enum class TaskbarState : VTInt
    {
        Clear = 0,
        Set = 1,
        Error = 2,
        Indeterminate = 3,
        Paused = 4
    };

    enum GraphicsOptions : VTInt
    {
        Off = 0,
        Intense = 1,
        // The 2 and 5 entries here are for BOTH the extended graphics options,
        // as well as the Faint/Blink options.
        RGBColorOrFaint = 2, // 2 is also Faint, decreased intensity (ISO 6429).
        Italics = 3,
        Underline = 4,
        BlinkOrXterm256Index = 5, // 5 is also Blink.
        RapidBlink = 6,
        Negative = 7,
        Invisible = 8,
        CrossedOut = 9,
        DoublyUnderlined = 21,
        NotIntenseOrFaint = 22,
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

    enum LogicalAttributeOptions : VTInt
    {
        Default = 0,
        Protected = 1,
        Unprotected = 2
    };

    // Many of these correspond directly to SGR parameters (the GraphicsOptions enum), but
    // these are distinct (notably 10 and 11, which as SGR parameters would select fonts,
    // are used here to indicate that the foreground/background colors should be saved).
    // From xterm's ctlseqs doc for XTPUSHSGR:
    //
    //      Ps = 1    =>  Intense.
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
    enum class SgrSaveRestoreStackOptions : VTInt
    {
        All = 0,
        Intense = 1,
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

    using ANSIStandardStatus = FlaggedEnumValue<0x00000000>;
    using DECPrivateStatus = FlaggedEnumValue<0x01000000>;

    enum class StatusType : VTInt
    {
        OS_OperatingStatus = ANSIStandardStatus(5),
        CPR_CursorPositionReport = ANSIStandardStatus(6),
        ExCPR_ExtendedCursorPositionReport = DECPrivateStatus(6),
        MSR_MacroSpaceReport = DECPrivateStatus(62),
        MEM_MemoryChecksum = DECPrivateStatus(63),
    };

    using ANSIStandardMode = FlaggedEnumValue<0x00000000>;
    using DECPrivateMode = FlaggedEnumValue<0x01000000>;

    enum ModeParams : VTInt
    {
        IRM_InsertReplaceMode = ANSIStandardMode(4),
        DECCKM_CursorKeysMode = DECPrivateMode(1),
        DECANM_AnsiMode = DECPrivateMode(2),
        DECCOLM_SetNumberOfColumns = DECPrivateMode(3),
        DECSCNM_ScreenMode = DECPrivateMode(5),
        DECOM_OriginMode = DECPrivateMode(6),
        DECAWM_AutoWrapMode = DECPrivateMode(7),
        DECARM_AutoRepeatMode = DECPrivateMode(8),
        ATT610_StartCursorBlink = DECPrivateMode(12),
        DECTCEM_TextCursorEnableMode = DECPrivateMode(25),
        XTERM_EnableDECCOLMSupport = DECPrivateMode(40),
        DECNKM_NumericKeypadMode = DECPrivateMode(66),
        DECBKM_BackarrowKeyMode = DECPrivateMode(67),
        VT200_MOUSE_MODE = DECPrivateMode(1000),
        BUTTON_EVENT_MOUSE_MODE = DECPrivateMode(1002),
        ANY_EVENT_MOUSE_MODE = DECPrivateMode(1003),
        FOCUS_EVENT_MODE = DECPrivateMode(1004),
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

    enum TabClearType : VTInt
    {
        ClearCurrentColumn = 0,
        ClearAllColumns = 3
    };

    enum WindowManipulationType : VTInt
    {
        Invalid = 0,
        DeIconifyWindow = 1,
        IconifyWindow = 2,
        RefreshWindow = 7,
        ResizeWindowInCharacters = 8,
    };

    enum class CursorStyle : VTInt
    {
        UserDefault = 0, // Implemented as "restore cursor to user default".
        BlinkingBlock = 1,
        SteadyBlock = 2,
        BlinkingUnderline = 3,
        SteadyUnderline = 4,
        BlinkingBar = 5,
        SteadyBar = 6
    };

    enum class ReportingPermission : VTInt
    {
        Unsolicited = 0,
        Solicited = 1
    };

    enum class LineFeedType : VTInt
    {
        WithReturn,
        WithoutReturn,
        DependsOnMode
    };

    enum class DrcsEraseControl : VTInt
    {
        AllChars = 0,
        ReloadedChars = 1,
        AllRenditions = 2
    };

    enum class DrcsCellMatrix : VTInt
    {
        Default = 0,
        Invalid = 1,
        Size5x10 = 2,
        Size6x10 = 3,
        Size7x10 = 4
    };

    enum class DrcsFontSet : VTInt
    {
        Default = 0,
        Size80x24 = 1,
        Size132x24 = 2,
        Size80x36 = 11,
        Size132x36 = 12,
        Size80x48 = 21,
        Size132x48 = 22
    };

    enum class DrcsFontUsage : VTInt
    {
        Default = 0,
        Text = 1,
        FullCell = 2
    };

    enum class DrcsCharsetSize : VTInt
    {
        Size94 = 0,
        Size96 = 1
    };

    enum class MacroDeleteControl : VTInt
    {
        DeleteId = 0,
        DeleteAll = 1
    };

    enum class MacroEncoding : VTInt
    {
        Text = 0,
        HexPair = 1
    };

    enum class ReportFormat : VTInt
    {
        TerminalStateReport = 1,
        ColorTableReport = 2
    };

    constexpr VTInt s_sDECCOLMSetColumns = 132;
    constexpr VTInt s_sDECCOLMResetColumns = 80;

    enum class MarkCategory : size_t
    {
        Prompt = 0,
        Error = 1,
        Warning = 2,
        Success = 3,
        Info = 4
    };

    struct ScrollMark
    {
        std::optional<til::color> color;
        til::point start;
        til::point end; // exclusive
        std::optional<til::point> commandEnd;
        std::optional<til::point> outputEnd;

        MarkCategory category{ MarkCategory::Info };
        // Other things we may want to think about in the future are listed in
        // GH#11000
    };
}
