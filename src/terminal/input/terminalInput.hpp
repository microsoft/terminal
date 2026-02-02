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

        // Kitty keyboard protocol progressive enhancement flags
        // https://sw.kovidgoyal.net/kitty/keyboard-protocol/
        struct KittyKeyboardProtocolFlags
        {
            static constexpr uint8_t None = 0;
            static constexpr uint8_t DisambiguateEscapeCodes = 1 << 0;
            static constexpr uint8_t ReportEventTypes = 1 << 1;
            static constexpr uint8_t ReportAlternateKeys = 1 << 2;
            static constexpr uint8_t ReportAllKeysAsEscapeCodes = 1 << 3;
            static constexpr uint8_t ReportAssociatedText = 1 << 4;
            static constexpr uint8_t All = (1 << 5) - 1;
        };
        enum class KittyKeyboardProtocolMode : uint8_t
        {
            Replace = 1,
            Set = 2,
            Reset = 3,
        };

        TerminalInput() noexcept;
        void UseAlternateScreenBuffer() noexcept;
        void UseMainScreenBuffer() noexcept;
        void SetInputMode(Mode mode, bool enabled) noexcept;
        bool GetInputMode(Mode mode) const noexcept;
        void ResetInputModes() noexcept;
        void ForceDisableWin32InputMode(bool win32InputMode) noexcept;
        void ForceDisableKittyKeyboardProtocol(bool disable) noexcept;

        // Kitty keyboard protocol methods
        void SetKittyKeyboardProtocol(uint8_t flags, KittyKeyboardProtocolMode mode) noexcept;
        uint8_t GetKittyFlags() const noexcept;
        void PushKittyFlags(uint8_t flags) noexcept;
        void PopKittyFlags(size_t count) noexcept;
        void ResetKittyKeyboardProtocols() noexcept;

#pragma region MouseInput
        // These methods are defined in mouseInput.cpp

        bool IsTrackingMouseInput() const noexcept;
        bool ShouldSendAlternateScroll(unsigned int button, short delta) const noexcept;
#pragma endregion

    private:
        struct CodepointBuffer
        {
            wchar_t buf[3];
            uint16_t len;
        };

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
        bool _inAlternateBuffer{ false };

        // Kitty keyboard protocol state
        static constexpr size_t KittyStackMaxSize = 8;
        bool _forceDisableKittyKeyboardProtocol = false;
        uint8_t _kittyFlags = 0;
        std::vector<uint8_t> _kittyMainStack;
        std::vector<uint8_t> _kittyAltStack;

        const wchar_t* _csi = L"\x1B[";
        const wchar_t* _ss3 = L"\x1BO";

        void _initKeyboardMap() noexcept;
        DWORD _trackControlKeyState(const KEY_EVENT_RECORD& key) noexcept;
        static std::array<byte, 256> _getKeyboardState(WORD virtualKeyCode, DWORD controlKeyState);
        [[nodiscard]] static wchar_t _makeCtrlChar(wchar_t ch);
        [[nodiscard]] StringType _makeCharOutput(wchar_t ch);
        [[nodiscard]] static StringType _makeNoOutput() noexcept;
        void _escapeOutput(StringType& charSequence, bool altIsPressed) const;
        [[nodiscard]] OutputType _makeWin32Output(const KEY_EVENT_RECORD& key) const;
        [[nodiscard]] OutputType _makeKittyOutput(const KEY_EVENT_RECORD& key, DWORD controlKeyState);
        static int32_t _getKittyKeyCode(const KEY_EVENT_RECORD& key, DWORD controlKeyState) noexcept;
        std::vector<uint8_t>& _getKittyStack() noexcept;
        static bool _codepointIsText(uint32_t cp) noexcept;
        static CodepointBuffer _codepointToBuffer(uint32_t cp) noexcept;
        static uint32_t _bufferToCodepoint(const wchar_t* str) noexcept;
        static uint32_t _codepointToLower(uint32_t cp) noexcept;
        static uint32_t _bufferToLowerCodepoint(wchar_t* buf, int cap) noexcept;
        static uint32_t _getBaseLayoutCodepoint(WORD vkey) noexcept;

#pragma region MouseInputState Management
        // These methods are defined in mouseInputState.cpp
        struct MouseInputState
        {
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

        static constexpr unsigned int s_GetPressedButton(MouseButtonState state) noexcept;
#pragma endregion
    };
}
