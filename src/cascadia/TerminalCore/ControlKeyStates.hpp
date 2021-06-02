// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// All the modifiers in this file EXCEPT the win key come from
// https://docs.microsoft.com/en-us/windows/console/key-event-record-str
//
// Since we also want to be able to encode win-key info in this structure, we'll
// add those values manually here
constexpr DWORD RIGHT_WIN_PRESSED = 0x0200;
constexpr DWORD LEFT_WIN_PRESSED = 0x0400;

namespace Microsoft::Terminal::Core
{
    class ControlKeyStates;
}

constexpr Microsoft::Terminal::Core::ControlKeyStates operator|(Microsoft::Terminal::Core::ControlKeyStates lhs, Microsoft::Terminal::Core::ControlKeyStates rhs) noexcept;

// This class is functionally equivalent to PowerShell's System.Management.Automation.Host.ControlKeyStates enum:
//   https://docs.microsoft.com/en-us/dotnet/api/system.management.automation.host.controlkeystates
// It's flagging values are compatible to those used by the NT console subsystem (<um/wincon.h>),
// as these are being used throughout older parts of this project.
class Microsoft::Terminal::Core::ControlKeyStates
{
    struct StaticValue
    {
        DWORD v;
    };

public:
    static constexpr StaticValue RightAltPressed{ RIGHT_ALT_PRESSED };
    static constexpr StaticValue LeftAltPressed{ LEFT_ALT_PRESSED };
    static constexpr StaticValue RightCtrlPressed{ RIGHT_CTRL_PRESSED };
    static constexpr StaticValue LeftCtrlPressed{ LEFT_CTRL_PRESSED };
    static constexpr StaticValue ShiftPressed{ SHIFT_PRESSED };
    static constexpr StaticValue NumlockOn{ NUMLOCK_ON };
    static constexpr StaticValue ScrolllockOn{ SCROLLLOCK_ON };
    static constexpr StaticValue CapslockOn{ CAPSLOCK_ON };
    static constexpr StaticValue EnhancedKey{ ENHANCED_KEY };
    static constexpr StaticValue RightWinPressed{ RIGHT_WIN_PRESSED };
    static constexpr StaticValue LeftWinPressed{ LEFT_WIN_PRESSED };

    constexpr ControlKeyStates() noexcept :
        _value(0) {}

    constexpr ControlKeyStates(StaticValue value) noexcept :
        _value(value.v) {}

    explicit constexpr ControlKeyStates(DWORD value) noexcept :
        _value(value) {}

    ControlKeyStates& operator|=(ControlKeyStates rhs) noexcept
    {
        _value |= rhs.Value();
        return *this;
    }

#ifdef WINRT_Windows_System_H
    ControlKeyStates(const winrt::Windows::System::VirtualKeyModifiers& modifiers) noexcept :
        _value{ 0 }
    {
        // static_cast to a uint32_t because we can't use the WI_IsFlagSet
        // macro directly with a VirtualKeyModifiers
        const auto m = static_cast<uint32_t>(modifiers);
        _value |= WI_IsFlagSet(m, static_cast<uint32_t>(winrt::Windows::System::VirtualKeyModifiers::Shift)) ?
                      SHIFT_PRESSED :
                      0;

        // Since we can't differentiate between the left & right versions of
        // Ctrl, Alt and Win in a VirtualKeyModifiers
        _value |= WI_IsFlagSet(m, static_cast<uint32_t>(winrt::Windows::System::VirtualKeyModifiers::Menu)) ?
                      LEFT_ALT_PRESSED :
                      0;
        _value |= WI_IsFlagSet(m, static_cast<uint32_t>(winrt::Windows::System::VirtualKeyModifiers::Control)) ?
                      LEFT_CTRL_PRESSED :
                      0;
        _value |= WI_IsFlagSet(m, static_cast<uint32_t>(winrt::Windows::System::VirtualKeyModifiers::Windows)) ?
                      LEFT_WIN_PRESSED :
                      0;
    }
#endif

    constexpr DWORD Value() const noexcept
    {
        return _value;
    }

    constexpr bool IsShiftPressed() const noexcept
    {
        return IsAnyFlagSet(ShiftPressed);
    }

    constexpr bool IsAltPressed() const noexcept
    {
        return IsAnyFlagSet(RightAltPressed | LeftAltPressed);
    }

    constexpr bool IsCtrlPressed() const noexcept
    {
        return IsAnyFlagSet(RightCtrlPressed | LeftCtrlPressed);
    }

    constexpr bool IsWinPressed() const noexcept
    {
        return IsAnyFlagSet(RightWinPressed | LeftWinPressed);
    }

    constexpr bool IsAltGrPressed() const noexcept
    {
        return AreAllFlagsSet(RightAltPressed | LeftCtrlPressed);
    }

    constexpr bool IsModifierPressed() const noexcept
    {
        return IsAnyFlagSet(RightAltPressed | LeftAltPressed | RightCtrlPressed | LeftCtrlPressed | ShiftPressed);
    }

private:
    constexpr bool AreAllFlagsSet(ControlKeyStates mask) const noexcept
    {
        return (Value() & mask.Value()) == mask.Value();
    }

    constexpr bool IsAnyFlagSet(ControlKeyStates mask) const noexcept
    {
        return (Value() & mask.Value()) != 0;
    }

    DWORD _value;
};

constexpr Microsoft::Terminal::Core::ControlKeyStates operator|(Microsoft::Terminal::Core::ControlKeyStates lhs, Microsoft::Terminal::Core::ControlKeyStates rhs) noexcept
{
    return Microsoft::Terminal::Core::ControlKeyStates{ lhs.Value() | rhs.Value() };
}

constexpr Microsoft::Terminal::Core::ControlKeyStates operator&(Microsoft::Terminal::Core::ControlKeyStates lhs, Microsoft::Terminal::Core::ControlKeyStates rhs) noexcept
{
    return Microsoft::Terminal::Core::ControlKeyStates{ lhs.Value() & rhs.Value() };
}

constexpr bool operator==(Microsoft::Terminal::Core::ControlKeyStates lhs, Microsoft::Terminal::Core::ControlKeyStates rhs) noexcept
{
    return lhs.Value() == rhs.Value();
}

constexpr bool operator!=(Microsoft::Terminal::Core::ControlKeyStates lhs, Microsoft::Terminal::Core::ControlKeyStates rhs) noexcept
{
    return lhs.Value() != rhs.Value();
}
