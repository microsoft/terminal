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
#pragma warning(suppress : 26495) // Variable '...' is uninitialized. Always initialize a member variable (type.6).
            explicit CodepointBuffer() noexcept = default;
            explicit CodepointBuffer(uint32_t cp) noexcept;

            void convertLowercase() noexcept;
            uint32_t asSingleCodepoint() const noexcept;

            wchar_t buf[4];
            int len = 0;
        };

        struct SanitizedKeyEvent
        {
            uint16_t virtualKey = 0;
            uint16_t scanCode = 0;
            uint32_t codepoint = 0;
            uint32_t controlKeyState = 0;
            bool leftCtrlIsReallyPressed = false;
            bool keyDown = false;
            bool keyRepeat = false;

            bool anyAltPressed() const noexcept;
            bool bothAltPressed() const noexcept;
            bool rightAltPressed() const noexcept;
            bool bothCtrlPressed() const noexcept;
            bool altGrPressed() const noexcept;
        };

        struct KeyboardHelper
        {
#pragma warning(suppress : 26495) // Variable '...' is uninitialized. Always initialize a member variable (type.6).
            explicit KeyboardHelper() noexcept = default;

            uint32_t getUnmodifiedKeyboardKey(const SanitizedKeyEvent& key) noexcept; // Without Ctrl/Alt
            uint32_t getKittyBaseKey(const SanitizedKeyEvent& key) noexcept; // Without Ctrl/Alt/Shift
            uint32_t getKittyShiftedKey(const SanitizedKeyEvent& key) noexcept; // Without Ctrl/Alt, with Shift
            uint32_t getKittyUSBaseKey(const SanitizedKeyEvent& key) noexcept; // Without Ctrl/Alt/Shift in US layout

        private:
            uint32_t getKeyboardKey(UINT vkey, DWORD controlKeyState, HKL hkl) noexcept;
            void init() noexcept;
            void initSlow() noexcept;

            bool _initialized = false;

            // Intentionally uninitialized until first use.
            HKL _keyboardLayout;
            uint8_t _keyboardState[256];
        };

        struct EncodingHelper
        {
            explicit EncodingHelper() noexcept;

            bool shiftPressed() const noexcept;
            bool altPressed() const noexcept;
            bool ctrlPressed() const noexcept;

            // The KKP CSI u sequence is a superset of other CSI sequences:
            //   CSI unicode-key-code:alternate-key-code-shift:alternate-key-code-base ; modifiers:event-type ; text-as-codepoint u
            uint32_t csiUnicodeKeyCode;
            uint32_t csiAltKeyCodeShifted; // KKP-specific
            uint32_t csiAltKeyCodeBase; // KKP-specific
            uint32_t csiModifier; // NOTE: The final VT sequence expects it this to be 1-based.
            uint32_t csiEventType; // KKP-specific
            uint32_t csiTextAsCodepoint; // KKP-specific
            // A non-zero csiFinal value indicates that this key
            // should be encoded as `CSI ... ; $csiFinal`.
            wchar_t csiFinal;

            // A non-zero ss3Final value indicates that this key
            // should be encoded as `ESC O $ss3Final`.
            wchar_t ss3Final;

            // If true, and Alt is pressed, an ESC prefix should be added to
            // the final sequence. This only applies to non-KKP encodings.
            bool altPrefix;

            // Any other encoding ends up as a non-zero plain value.
            // For instance, the Tab key gets translated to a plain "\t".
            std::wstring_view plain;
        };

        // storage location for the leading surrogate of a utf-16 surrogate pair
        wchar_t _leadingSurrogate = 0;

        std::optional<WORD> _lastVirtualKeyCode;
        DWORD _lastControlKeyState = 0;
        uint64_t _lastLeftCtrlTime = 0;
        uint64_t _lastRightAltTime = 0;

        til::enumset<Mode> _inputMode{ Mode::Ansi, Mode::AutoRepeat, Mode::AlternateScroll };
        bool _forceDisableWin32InputMode{ false };
        bool _inAlternateBuffer{ false };

        // Kitty keyboard protocol state
        static constexpr size_t KittyStackMaxSize = 8;
        bool _forceDisableKittyKeyboardProtocol = false;
        uint8_t _kittyFlags = 0;
        std::vector<uint8_t> _kittyMainStack;
        std::vector<uint8_t> _kittyAltStack;

        std::wstring_view _csi;
        std::wstring_view _ss3;
        std::wstring_view _focusInSequence;
        std::wstring_view _focusOutSequence;

        void _initKeyboardMap() noexcept;
        DWORD _trackControlKeyState(const KEY_EVENT_RECORD& key) noexcept;
        [[nodiscard]] static uint32_t _makeCtrlChar(uint32_t ch) noexcept;
        [[nodiscard]] static StringType _makeCharOutput(uint32_t ch);
        [[nodiscard]] static StringType _makeNoOutput() noexcept;
        [[nodiscard]] OutputType _makeWin32Output(const KEY_EVENT_RECORD& key);
        bool _encodeKitty(KeyboardHelper& kbd, EncodingHelper& enc, const SanitizedKeyEvent& key) noexcept;
        static uint32_t _getKittyFunctionalKeyCode(UINT vkey, WORD scanCode, bool enhanced) noexcept;
        void _encodeRegular(EncodingHelper& enc, const SanitizedKeyEvent& key) const noexcept;
        bool _formatEncodingHelper(EncodingHelper& enc, std::wstring& str) const;
        void _formatFallback(KeyboardHelper& kbd, const EncodingHelper& enc, const SanitizedKeyEvent& key, std::wstring& seq) const;
        static void _stringPushCodepoint(std::wstring& str, uint32_t cp);
        static uint32_t _codepointToLower(uint32_t cp) noexcept;

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
