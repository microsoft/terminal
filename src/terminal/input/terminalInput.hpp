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

        [[nodiscard]] static OutputType MakeUnhandled() noexcept;
        [[nodiscard]] static OutputType MakeOutput(const std::wstring_view& str);
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
            SendC1,

            Utf8MouseEncoding,
            SgrMouseEncoding,

            DefaultMouseTracking,
            ButtonEventMouseTracking,
            AnyEventMouseTracking,

            FocusEvent,

            AlternateScroll
        };

        TerminalInput() noexcept;
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
        wchar_t _leadingSurrogate = 0;

        std::optional<WORD> _lastVirtualKeyCode;
        DWORD _lastControlKeyState = 0;
        uint64_t _lastLeftCtrlTime = 0;
        uint64_t _lastRightAltTime = 0;
        std::unordered_map<int, std::wstring> _keyMap;
        std::wstring _focusInSequence;
        std::wstring _focusOutSequence;

        til::enumset<Mode> _inputMode{ Mode::Ansi, Mode::AutoRepeat, Mode::AlternateScroll };
        bool _forceDisableWin32InputMode{ false };

        const wchar_t* _csi = L"\x1B[";
        const wchar_t* _ss3 = L"\x1BO";

        void _initKeyboardMap() noexcept;
        DWORD _trackControlKeyState(const KEY_EVENT_RECORD& key);
        std::array<byte, 256> _getKeyboardState(const WORD virtualKeyCode, const DWORD controlKeyState) const;
        [[nodiscard]] static wchar_t _makeCtrlChar(const wchar_t ch);
        [[nodiscard]] StringType _makeCharOutput(wchar_t ch);
        [[nodiscard]] static StringType _makeNoOutput() noexcept;
        [[nodiscard]] void _escapeOutput(StringType& charSequence, const bool altIsPressed) const;
        [[nodiscard]] OutputType _makeWin32Output(const KEY_EVENT_RECORD& key) const;

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
        [[nodiscard]] OutputType _GenerateDefaultSequence(til::point position, unsigned int button, bool isHover, short modifierKeyState, short delta);
        [[nodiscard]] OutputType _GenerateUtf8Sequence(til::point position, unsigned int button, bool isHover, short modifierKeyState, short delta);
        [[nodiscard]] OutputType _GenerateSGRSequence(til::point position, unsigned int button, bool isRelease, bool isHover, short modifierKeyState, short delta);

        [[nodiscard]] OutputType _makeAlternateScrollOutput(unsigned int button, short delta) const;

        static constexpr unsigned int s_GetPressedButton(const MouseButtonState state) noexcept;
#pragma endregion
    };
}
