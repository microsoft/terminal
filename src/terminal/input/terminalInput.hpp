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
        void PushKittyFlags(uint8_t flags);
        void PopKittyFlags(size_t count);
        void ResetKittyKeyboardProtocols() noexcept;

#pragma region MouseInput
        // These methods are defined in mouseInput.cpp

        bool IsTrackingMouseInput() const noexcept;
        bool ShouldSendAlternateScroll(unsigned int button, short delta) const noexcept;
#pragma endregion

    private:
        struct CodepointBuffer
        {
            wchar_t buf[4];
            int len;
        };

        struct EncodingHelper
        {
            explicit EncodingHelper()
            {
                memset(this, 0, sizeof(*this));
            }

            void disableCtrlAltInKeyboardState() noexcept
            {
                keyboardState[VK_CONTROL] = 0;
                keyboardState[VK_MENU] = 0;
                keyboardState[VK_LCONTROL] = 0;
                keyboardState[VK_RCONTROL] = 0;
                keyboardState[VK_LMENU] = 0;
                keyboardState[VK_RMENU] = 0;
            }
            CodepointBuffer getKeyboardKey(UINT vkey, DWORD controlKeyState, HKL hkl) noexcept
            {
                CodepointBuffer cb;

                setupKeyboardState(controlKeyState);

                keyboardState[vkey] = 0x80;
                cb.len = ToUnicodeEx(vkey, 0, keyboardState, cb.buf, ARRAYSIZE(cb.buf), 4, hkl);
                keyboardState[vkey] = 0;

                return cb;
            }
            HKL getKeyboardLayoutCached() noexcept
            {
                if (!keyboardLayoutCached)
                {
                    keyboardLayout = getKeyboardLayout();
                    keyboardLayoutCached = true;
                }
                return keyboardLayout;
            }
            static HKL getKeyboardLayout() noexcept
            {
                // We need the current keyboard layout and state to look up the character
                // that would be transmitted in that state (via the ToUnicodeEx API).
                return GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
            }
            void setupKeyboardState(DWORD controlKeyState) noexcept
            {
                const uint8_t rightAlt = WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED) ? 0x80 : 0;
                const uint8_t leftAlt = WI_IsFlagSet(controlKeyState, LEFT_ALT_PRESSED) ? 0x80 : 0;
                const uint8_t rightCtrl = WI_IsFlagSet(controlKeyState, RIGHT_CTRL_PRESSED) ? 0x80 : 0;
                const uint8_t leftCtrl = WI_IsFlagSet(controlKeyState, LEFT_CTRL_PRESSED) ? 0x80 : 0;
                const uint8_t leftShift = WI_IsFlagSet(controlKeyState, SHIFT_PRESSED) ? 0x80 : 0;
                const uint8_t capsLock = WI_IsFlagSet(controlKeyState, CAPSLOCK_ON) ? 0x01 : 0;

                keyboardState[VK_SHIFT] = leftShift;
                keyboardState[VK_CONTROL] = leftCtrl | rightCtrl;
                keyboardState[VK_MENU] = leftAlt | rightAlt;
                keyboardState[VK_CAPITAL] = capsLock;
                keyboardState[VK_LSHIFT] = leftShift;
                keyboardState[VK_LCONTROL] = leftCtrl;
                keyboardState[VK_RCONTROL] = rightCtrl;
                keyboardState[VK_LMENU] = leftAlt;
                keyboardState[VK_RMENU] = rightAlt;
            }

            HKL keyboardLayout;
            uint8_t keyboardState[256];
            uint32_t codepointWithoutCtrlAlt;

            bool keyboardLayoutCached;

            // A non-zero csiFinal value indicates that this key
            // should be encoded as `CSI $csiParam1 ; $csiFinal`.
            wchar_t csiFinal;
            // The longest sequence we currently have is Kitty's with 6 parameters:
            //   CSI unicode-key-code:alternate-key-code-shift:alternate-key-code-base ; modifiers:event-type ; text-as-codepoint u
            // That's 6 parameters, but we can greatly simplify our logic if we just make it 3x3.
            uint32_t csiParam[3][3];

            // A non-zero ss3Final value indicates that this key
            // should be encoded as `ESC O $ss3Final`.
            wchar_t ss3Final;

            // Any other encoding ends up as a non-zero plain value.
            // For instance, the Tab key gets translated to a plain "\t".
            std::wstring_view plain;
            // If true, and Alt is pressed, an ESC prefix should be added to
            // the final sequence. This only applies to non-KKP encodings.
            bool plainAltPrefix;
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

        static constexpr std::wstring_view _csi{ L"\x1B[" };
        static constexpr std::wstring_view _ss3{ L"\x1BO" };

        void _initKeyboardMap() noexcept;
        DWORD _trackControlKeyState(const KEY_EVENT_RECORD& key) noexcept;
        static std::array<byte, 256> _getKeyboardState(size_t virtualKeyCode, DWORD controlKeyState);
        [[nodiscard]] static uint32_t _makeCtrlChar(uint32_t ch) noexcept;
        [[nodiscard]] static StringType _makeCharOutput(uint32_t ch);
        [[nodiscard]] static StringType _makeNoOutput() noexcept;
        [[nodiscard]] OutputType _makeWin32Output(const KEY_EVENT_RECORD& key) const;
        void _fillRegularKeyEncodingInfo(EncodingHelper& enc, const KEY_EVENT_RECORD& key, DWORD simpleKeyState) const noexcept;
        static uint32_t _getKittyFunctionalKeyCode(UINT vkey, WORD scanCode, DWORD simpleKeyState) noexcept;
        std::vector<uint8_t>& _getKittyStack() noexcept;
        static bool _codepointIsText(uint32_t cp) noexcept;
        static void _stringPushCodepoint(std::wstring& str, uint32_t cp);
        static CodepointBuffer _codepointToBuffer(uint32_t cp) noexcept;
        static uint32_t _bufferToCodepoint(const wchar_t* str) noexcept;
        static uint32_t _codepointToLower(uint32_t cp) noexcept;
        static uint32_t _bufferToLowerCodepoint(wchar_t* buf, int cap) noexcept;
        static uint32_t _getBaseLayoutCodepoint(WORD scanCode) noexcept;

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
