/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- terminalInput.hpp

Abstract:
- This serves as an adapter between virtual key input from a user and the virtual terminal sequences that are
  typically emitted by an xterm-compatible console.

Author(s):
- Michael Niksa (MiNiksa) 30-Oct-2015
--*/

#include <functional>
#include "../../types/inc/IInputEvent.hpp"
#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class TerminalInput final
    {
    public:
        TerminalInput(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn);

        TerminalInput() = delete;
        TerminalInput(const TerminalInput& old) = default;
        TerminalInput(TerminalInput&& moved) = default;

        TerminalInput& operator=(const TerminalInput& old) = default;
        TerminalInput& operator=(TerminalInput&& moved) = default;

        ~TerminalInput() = default;

        bool HandleKey(const IInputEvent* const pInEvent) const;
        bool HandleChar(const wchar_t ch);
        void ChangeKeypadMode(const bool applicationMode) noexcept;
        void ChangeCursorKeysMode(const bool applicationMode) noexcept;

#pragma region MouseInput
        // These methods are defined in mouseInput.cpp
        bool HandleMouse(const COORD position,
                         const unsigned int button,
                         const short modifierKeyState,
                         const short delta);
#pragma endregion

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        void SetUtf8ExtendedMode(const bool enable) noexcept;
        void SetSGRExtendedMode(const bool enable) noexcept;

        void EnableDefaultTracking(const bool enable) noexcept;
        void EnableButtonEventTracking(const bool enable) noexcept;
        void EnableAnyEventTracking(const bool enable) noexcept;

        void EnableAlternateScroll(const bool enable) noexcept;
        void UseAlternateScreenBuffer() noexcept;
        void UseMainScreenBuffer() noexcept;
#pragma endregion

    private:
        std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteEvents;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        bool _keypadApplicationMode = false;
        bool _cursorApplicationMode = false;

        void _SendNullInputSequence(const DWORD dwControlKeyState) const;
        void _SendInputSequence(const std::wstring_view sequence) const noexcept;
        void _SendEscapedInputSequence(const wchar_t wch) const;

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        enum class ExtendedMode : unsigned int
        {
            None,
            Utf8,
            Sgr,
            Urxvt
        };

        enum class TrackingMode : unsigned int
        {
            None,
            Default,
            ButtonEvent,
            AnyEvent
        };

        struct MouseInputState
        {
            ExtendedMode extendedMode{ ExtendedMode::None };
            TrackingMode trackingMode{ TrackingMode::None };
            bool alternateScroll{ false };
            bool inAlternateBuffer{ false };
            COORD lastPos{ -1, -1 };
            unsigned int lastButton{ 0 };
        };

        MouseInputState _mouseInputState;
#pragma endregion

#pragma region MouseInput
        static std::wstring _GenerateDefaultSequence(const COORD position,
                                                     const unsigned int button,
                                                     const bool isHover,
                                                     const short modifierKeyState,
                                                     const short delta);
        static std::wstring _GenerateUtf8Sequence(const COORD position,
                                                  const unsigned int button,
                                                  const bool isHover,
                                                  const short modifierKeyState,
                                                  const short delta);
        static std::wstring _GenerateSGRSequence(const COORD position,
                                                 const unsigned int button,
                                                 const bool isDown,
                                                 const bool isHover,
                                                 const short modifierKeyState,
                                                 const short delta);

        bool _ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept;
        bool _SendAlternateScroll(const short delta) const noexcept;

        static unsigned int s_GetPressedButton() noexcept;
#pragma endregion
    };
}
