/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- InputStateMachineEngine.hpp

Abstract:
- This is the implementation of the client VT input state machine engine.
    This generates InputEvents from a stream of VT sequences emitted by a
    client "terminal" application.

Author(s):
- Mike Griese (migrie) 18 Aug 2017
--*/
#pragma once

#include "IStateMachineEngine.hpp"
#include <functional>
#include "../../types/inc/IInputEvent.hpp"
#include "../adapter/IInteractDispatch.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    // The values used by VkKeyScan to encode modifiers in the high order byte
    constexpr short KEYSCAN_SHIFT = 1;
    constexpr short KEYSCAN_CTRL = 2;
    constexpr short KEYSCAN_ALT = 4;

    // The values with which VT encodes modifier values.
    constexpr short VT_SHIFT = 1;
    constexpr short VT_ALT = 2;
    constexpr short VT_CTRL = 4;

    // The assumed values for SGR Mouse Scroll Wheel deltas
    constexpr DWORD SCROLL_DELTA_BACKWARD = 0xFF800000;
    constexpr DWORD SCROLL_DELTA_FORWARD = 0x00800000;

    constexpr size_t WRAPPED_SEQUENCE_MAX_LENGTH = 8;

    // For reference, the equivalent INPUT_RECORD values are:
    // RIGHT_ALT_PRESSED   0x0001
    // LEFT_ALT_PRESSED    0x0002
    // RIGHT_CTRL_PRESSED  0x0004
    // LEFT_CTRL_PRESSED   0x0008
    // SHIFT_PRESSED       0x0010
    // NUMLOCK_ON          0x0020
    // SCROLLLOCK_ON       0x0040
    // CAPSLOCK_ON         0x0080
    // ENHANCED_KEY        0x0100

    enum class DeviceAttribute : uint64_t
    {
        // Special value to indicate that InputStateMachineEngine::_deviceAttributes has been set.
        // 0 in this case means 1<<0 == 1, which in turn means that _deviceAttributes is non-zero.
        __some__ = 0,

        Columns132 = 1,
        PrinterPort = 2,
        Sixel = 4,
        SelectiveErase = 6,
        SoftCharacterSet = 7,
        UserDefinedKeys = 8,
        NationalReplacementCharacterSets = 9,
        SerboCroatianCharacterSet = 12,
        EightBitInterfaceArchitecture = 14,
        TechnicalCharacterSet = 15,
        WindowingCapability = 18,
        Sessions = 19,
        HorizontalScrolling = 21,
        Color = 22,
        GreekCharacterSet = 23,
        TurkishCharacterSet = 24,
        RectangularAreaOperations = 28,
        TextMacros = 32,
        Latin2CharacterSet = 42,
        PCTerm = 44,
        SoftKeyMapping = 45,
        AsciiTerminalEmulation = 46,
    };

    enum CsiActionCodes : uint64_t
    {
        ArrowUp = VTID("A"),
        ArrowDown = VTID("B"),
        ArrowRight = VTID("C"),
        ArrowLeft = VTID("D"),
        Home = VTID("H"),
        End = VTID("F"),
        FocusIn = VTID("I"),
        FocusOut = VTID("O"),
        MouseDown = VTID("<M"),
        MouseUp = VTID("<m"),
        Generic = VTID("~"), // Used for a whole bunch of possible keys
        CSI_F1 = VTID("P"),
        CSI_F2 = VTID("Q"),
        CSI_F3 = VTID("R"), // Both F3 and DSR are on R.
        // DSR_DeviceStatusReportResponse = VTID("R"),
        CSI_F4 = VTID("S"),
        DA_DeviceAttributes = VTID("?c"),
        DTTERM_WindowManipulation = VTID("t"),
        CursorBackTab = VTID("Z"),
        Win32KeyboardInput = VTID("_")
    };

    enum CsiMouseButtonCodes : unsigned short
    {
        Left = 0,
        Middle = 1,
        Right = 2,
        Released = 3,
        ScrollForward = 4,
        ScrollBack = 5,
    };

    constexpr unsigned short CsiMouseModifierCode_Drag = 32;

    enum CsiMouseModifierCodes : unsigned short
    {
        Shift = 4,
        Meta = 8,
        Ctrl = 16,
        Drag = 32,
    };

    // Sequences ending in '~' use these numbers as identifiers.
    enum class GenericKeyIdentifiers : size_t
    {
        GenericHome = 1,
        Insert = 2,
        Delete = 3,
        GenericEnd = 4,
        Prior = 5, //PgUp
        Next = 6, //PgDn
        F5 = 15,
        F6 = 17,
        F7 = 18,
        F8 = 19,
        F9 = 20,
        F10 = 21,
        F11 = 23,
        F12 = 24,
    };

    enum class Ss3ActionCodes : wchar_t
    {
        ArrowUp = L'A',
        ArrowDown = L'B',
        ArrowRight = L'C',
        ArrowLeft = L'D',
        Home = L'H',
        End = L'F',
        SS3_F1 = L'P',
        SS3_F2 = L'Q',
        SS3_F3 = L'R',
        SS3_F4 = L'S',
    };

    class InputStateMachineEngine : public IStateMachineEngine
    {
    public:
        InputStateMachineEngine(std::unique_ptr<IInteractDispatch> pDispatch, const bool lookingForDSR = false);

        til::enumset<DeviceAttribute, uint64_t> WaitUntilDA1(DWORD timeout) const noexcept;

        bool EncounteredWin32InputModeSequence() const noexcept override;

        bool ActionExecute(const wchar_t wch) override;
        bool ActionExecuteFromEscape(const wchar_t wch) override;

        bool ActionPrint(const wchar_t wch) override;

        bool ActionPrintString(const std::wstring_view string) override;

        bool ActionPassThroughString(const std::wstring_view string) override;

        bool ActionEscDispatch(const VTID id) override;

        bool ActionVt52EscDispatch(const VTID id, const VTParameters parameters) noexcept override;

        bool ActionCsiDispatch(const VTID id, const VTParameters parameters) override;

        StringHandler ActionDcsDispatch(const VTID id, const VTParameters parameters) noexcept override;

        bool ActionOscDispatch(const size_t parameter, const std::wstring_view string) noexcept override;

        bool ActionSs3Dispatch(const wchar_t wch, const VTParameters parameters) override;

    private:
        const std::unique_ptr<IInteractDispatch> _pDispatch;
        std::atomic<uint64_t> _deviceAttributes{ 0 };
        bool _lookingForDSR = false;
        bool _encounteredWin32InputModeSequence = false;
        bool _expectingStringTerminator = false;
        DWORD _mouseButtonState = 0;
        std::chrono::milliseconds _doubleClickTime;
        std::optional<til::point> _lastMouseClickPos{};
        std::optional<std::chrono::steady_clock::time_point> _lastMouseClickTime{};
        std::optional<size_t> _lastMouseClickButton{};

        DWORD _GetCursorKeysModifierState(const VTParameters parameters, const VTID id) noexcept;
        DWORD _GetGenericKeysModifierState(const VTParameters parameters) noexcept;
        DWORD _GetSGRMouseModifierState(const size_t modifierParam) noexcept;
        bool _GenerateKeyFromChar(const wchar_t wch, short& vkey, DWORD& modifierState) noexcept;

        DWORD _GetModifier(const size_t parameter) noexcept;

        bool _UpdateSGRMouseButtonState(const VTID id,
                                        const size_t sgrEncoding,
                                        DWORD& buttonState,
                                        DWORD& eventFlags,
                                        const til::point uiPos);
        bool _GetGenericVkey(const GenericKeyIdentifiers identifier, short& vkey) const;
        bool _GetCursorKeysVkey(const VTID id, short& vkey) const;
        bool _GetSs3KeysVkey(const wchar_t wch, short& vkey) const;

        void _WriteSingleKey(const short vkey, const DWORD modifierState);
        void _WriteSingleKey(const wchar_t wch, const short vkey, const DWORD modifierState);

        void _WriteMouseEvent(const til::point uiPos, const DWORD buttonState, const DWORD controlKeyState, const DWORD eventFlags);

        void _GenerateWrappedSequence(const wchar_t wch,
                                      const short vkey,
                                      const DWORD modifierState,
                                      InputEventQueue& input);

        void _GetSingleKeypress(const wchar_t wch,
                                const short vkey,
                                const DWORD modifierState,
                                InputEventQueue& input);

        bool _GetWindowManipulationType(const std::span<const size_t> parameters,
                                        unsigned int& function) const noexcept;

        static INPUT_RECORD _GenerateWin32Key(const VTParameters& parameters);

        bool _DoControlCharacter(const wchar_t wch, const bool writeAlt);

#ifdef UNIT_TESTING
        friend class InputEngineTest;
#endif
    };
}
