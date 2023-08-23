// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::VirtualTerminal
{
    class TerminalInput final
    {
    public:
        using StringType = std::wstring;
        using OutputType = std::optional<StringType>;

        struct MouseButtonState
        {
            bool isLeftButtonDown;
            bool isMiddleButtonDown;
            bool isRightButtonDown;
        };

        static [[nodiscard]] OutputType MakeUnhandled() noexcept;
        static [[nodiscard]] OutputType MakeOutput(const std::wstring_view& str);
        [[nodiscard]] OutputType HandleKey(const INPUT_RECORD& pInEvent);
        [[nodiscard]] OutputType HandleFocus(bool focused) const;
        [[nodiscard]] OutputType HandleMouse(til::point position, unsigned int button, short modifierKeyState, short delta, MouseButtonState state);

        enum class Mode : size_t
        {
            LineFeed,
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

        bool IsTrackingMouseInput() const noexcept;
        bool ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept;
#pragma endregion

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        void UseAlternateScreenBuffer() noexcept;
        void UseMainScreenBuffer() noexcept;
#pragma endregion

    private:
        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        std::optional<WORD> _lastVirtualKeyCode;

        til::enumset<Mode> _inputMode{ Mode::Ansi, Mode::AutoRepeat };
        bool _forceDisableWin32InputMode{ false };

        [[nodiscard]] OutputType _makeCharOutput(wchar_t ch);
        static [[nodiscard]] OutputType _makeEscapedOutput(wchar_t wch);
        static [[nodiscard]] OutputType _makeWin32Output(const KEY_EVENT_RECORD& key);
        static [[nodiscard]] OutputType _searchWithModifier(const KEY_EVENT_RECORD& keyEvent);

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
        static [[nodiscard]] OutputType _GenerateDefaultSequence(til::point position, unsigned int button, bool isHover, short modifierKeyState, short delta);
        static [[nodiscard]] OutputType _GenerateUtf8Sequence(til::point position, unsigned int button, bool isHover, short modifierKeyState, short delta);
        static [[nodiscard]] OutputType _GenerateSGRSequence(til::point position, unsigned int button, bool isDown, bool isHover, short modifierKeyState, short delta);

        [[nodiscard]] OutputType _makeAlternateScrollOutput(short delta) const;

        static constexpr unsigned int s_GetPressedButton(const MouseButtonState state) noexcept;
#pragma endregion
    };
}
