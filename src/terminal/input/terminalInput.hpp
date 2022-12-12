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

        bool HandleKey(const IInputEvent* const pInEvent);
        bool HandleFocus(const bool focused) noexcept;

        enum class Mode : size_t
        {
            Ansi,
            AutoRepeat,
            Keypad,
            CursorKey,
            BackarrowKey,
            Win32,

            Utf8MouseEncoding,
            SgrMouseEncoding,

            DefaultMouseTracking,
            ButtonEventMouseTracking,
            AnyEventMouseTracking,

            FocusEvent,

            AlternateScroll
        };

        void SetInputMode(const Mode mode, const bool enabled) noexcept;
        bool GetInputMode(const Mode mode) const noexcept;
        void ResetInputModes() noexcept;
        void ForceDisableWin32InputMode(const bool win32InputMode) noexcept;

#pragma region MouseInput
        // These methods are defined in mouseInput.cpp

        struct MouseButtonState
        {
            bool isLeftButtonDown;
            bool isMiddleButtonDown;
            bool isRightButtonDown;
        };

        bool HandleMouse(const til::point position,
                         const unsigned int button,
                         const short modifierKeyState,
                         const short delta,
                         const MouseButtonState state);

        bool IsTrackingMouseInput() const noexcept;
        bool ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept;
#pragma endregion

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        void UseAlternateScreenBuffer() noexcept;
        void UseMainScreenBuffer() noexcept;
#pragma endregion

    private:
        std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> _pfnWriteEvents;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        std::optional<WORD> _lastVirtualKeyCode;

        til::enumset<Mode> _inputMode{ Mode::Ansi, Mode::AutoRepeat };
        bool _forceDisableWin32InputMode{ false };

        void _SendChar(const wchar_t ch);
        void _SendNullInputSequence(const DWORD dwControlKeyState) const;
        void _SendInputSequence(const std::wstring_view sequence) const noexcept;
        void _SendEscapedInputSequence(const wchar_t wch) const;
        static std::wstring _GenerateWin32KeySequence(const KeyEvent& key);

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        struct MouseInputState
        {
            bool inAlternateBuffer{ false };
            til::point lastPos{ -1, -1 };
            unsigned int lastButton{ 0 };
            int accumulatedDelta{ 0 };
        };

        MouseInputState _mouseInputState;
#pragma endregion

#pragma region MouseInput
        static std::wstring _GenerateDefaultSequence(const til::point position,
                                                     const unsigned int button,
                                                     const bool isHover,
                                                     const short modifierKeyState,
                                                     const short delta);
        static std::wstring _GenerateUtf8Sequence(const til::point position,
                                                  const unsigned int button,
                                                  const bool isHover,
                                                  const short modifierKeyState,
                                                  const short delta);
        static std::wstring _GenerateSGRSequence(const til::point position,
                                                 const unsigned int button,
                                                 const bool isDown,
                                                 const bool isHover,
                                                 const short modifierKeyState,
                                                 const short delta);

        bool _SendAlternateScroll(const short delta) const noexcept;

        static constexpr unsigned int s_GetPressedButton(const MouseButtonState state) noexcept;
#pragma endregion
    };
}
