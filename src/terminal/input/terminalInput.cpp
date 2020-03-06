// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"

#include "strsafe.h"

#define WIL_SUPPORT_BITOPERATION_PASCAL_NAMES
#include <wil\Common.h>

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\..\interactivity\inc\VtApiRedirection.hpp"
#endif

#include "..\..\inc\unicode.hpp"
#include "..\..\types\inc\Utf16Parser.hpp"

using namespace Microsoft::Console::VirtualTerminal;

DWORD const dwAltGrFlags = LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED;

TerminalInput::TerminalInput(_In_ std::function<void(std::deque<std::unique_ptr<IInputEvent>>&)> pfn) :
    _leadingSurrogate{}
{
    _pfnWriteEvents = pfn;
}

struct TermKeyMap
{
    const WORD vkey;
    const std::wstring_view sequence;
    const DWORD modifiers;

    constexpr TermKeyMap(WORD vkey, std::wstring_view sequence) noexcept :
        TermKeyMap(vkey, 0, sequence)
    {
    }

    constexpr TermKeyMap(const WORD vkey, const DWORD modifiers, std::wstring_view sequence) noexcept :
        vkey(vkey),
        sequence(sequence),
        modifiers(modifiers)
    {
    }
};

// See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-PC-Style-Function-Keys
//    For the source for these tables.
// Also refer to the values in terminfo for kcub1, kcud1, kcuf1, kcuu1, kend, khome.
//   the 'xterm' setting lists the application mode versions of these sequences.
static constexpr std::array<TermKeyMap, 6> s_cursorKeysNormalMapping = {
    TermKeyMap{ VK_UP, L"\x1b[A" },
    TermKeyMap{ VK_DOWN, L"\x1b[B" },
    TermKeyMap{ VK_RIGHT, L"\x1b[C" },
    TermKeyMap{ VK_LEFT, L"\x1b[D" },
    TermKeyMap{ VK_HOME, L"\x1b[H" },
    TermKeyMap{ VK_END, L"\x1b[F" },
};

static constexpr std::array<TermKeyMap, 6> s_cursorKeysApplicationMapping{
    TermKeyMap{ VK_UP, L"\x1bOA" },
    TermKeyMap{ VK_DOWN, L"\x1bOB" },
    TermKeyMap{ VK_RIGHT, L"\x1bOC" },
    TermKeyMap{ VK_LEFT, L"\x1bOD" },
    TermKeyMap{ VK_HOME, L"\x1bOH" },
    TermKeyMap{ VK_END, L"\x1bOF" },
};

static constexpr std::array<TermKeyMap, 20> s_keypadNumericMapping{
    TermKeyMap{ VK_TAB, L"\x09" },
    TermKeyMap{ VK_BACK, L"\x7f" },
    TermKeyMap{ VK_PAUSE, L"\x1a" },
    TermKeyMap{ VK_ESCAPE, L"\x1b" },
    TermKeyMap{ VK_INSERT, L"\x1b[2~" },
    TermKeyMap{ VK_DELETE, L"\x1b[3~" },
    TermKeyMap{ VK_PRIOR, L"\x1b[5~" },
    TermKeyMap{ VK_NEXT, L"\x1b[6~" },
    TermKeyMap{ VK_F1, L"\x1bOP" }, // also \x1b[11~, PuTTY uses \x1b\x1b[A
    TermKeyMap{ VK_F2, L"\x1bOQ" }, // also \x1b[12~, PuTTY uses \x1b\x1b[B
    TermKeyMap{ VK_F3, L"\x1bOR" }, // also \x1b[13~, PuTTY uses \x1b\x1b[C
    TermKeyMap{ VK_F4, L"\x1bOS" }, // also \x1b[14~, PuTTY uses \x1b\x1b[D
    TermKeyMap{ VK_F5, L"\x1b[15~" },
    TermKeyMap{ VK_F6, L"\x1b[17~" },
    TermKeyMap{ VK_F7, L"\x1b[18~" },
    TermKeyMap{ VK_F8, L"\x1b[19~" },
    TermKeyMap{ VK_F9, L"\x1b[20~" },
    TermKeyMap{ VK_F10, L"\x1b[21~" },
    TermKeyMap{ VK_F11, L"\x1b[23~" },
    TermKeyMap{ VK_F12, L"\x1b[24~" },
};

//Application mode - Some terminals support both a "Numeric" input mode, and an "Application" mode
//  The standards vary on what each key translates to in the various modes, so I tried to make it as close
//  to the VT220 standard as possible.
//  The notable difference is in the arrow keys, which in application mode translate to "^[0A" (etc) as opposed to "^[[A" in numeric
//Some very unclear documentation at http://invisible-island.net/xterm/ctlseqs/ctlseqs.html also suggests alternate encodings for F1-4
//  which I have left in the comments on those entries as something to possibly add in the future, if need be.
//It seems to me as though this was used for early numpad implementations, where presently numlock would enable
//  "numeric" mode, outputting the numbers on the keys, while "application" mode does things like pgup/down, arrow keys, etc.
//These keys aren't translated at all in numeric mode, so I figured I'd leave them out of the numeric table.
static constexpr std::array<TermKeyMap, 20> s_keypadApplicationMapping{
    TermKeyMap{ VK_TAB, L"\x09" },
    TermKeyMap{ VK_BACK, L"\x7f" },
    TermKeyMap{ VK_PAUSE, L"\x1a" },
    TermKeyMap{ VK_ESCAPE, L"\x1b" },
    TermKeyMap{ VK_INSERT, L"\x1b[2~" },
    TermKeyMap{ VK_DELETE, L"\x1b[3~" },
    TermKeyMap{ VK_PRIOR, L"\x1b[5~" },
    TermKeyMap{ VK_NEXT, L"\x1b[6~" },
    TermKeyMap{ VK_F1, L"\x1bOP" }, // also \x1b[11~, PuTTY uses \x1b\x1b[A
    TermKeyMap{ VK_F2, L"\x1bOQ" }, // also \x1b[12~, PuTTY uses \x1b\x1b[B
    TermKeyMap{ VK_F3, L"\x1bOR" }, // also \x1b[13~, PuTTY uses \x1b\x1b[C
    TermKeyMap{ VK_F4, L"\x1bOS" }, // also \x1b[14~, PuTTY uses \x1b\x1b[D
    TermKeyMap{ VK_F5, L"\x1b[15~" },
    TermKeyMap{ VK_F6, L"\x1b[17~" },
    TermKeyMap{ VK_F7, L"\x1b[18~" },
    TermKeyMap{ VK_F8, L"\x1b[19~" },
    TermKeyMap{ VK_F9, L"\x1b[20~" },
    TermKeyMap{ VK_F10, L"\x1b[21~" },
    TermKeyMap{ VK_F11, L"\x1b[23~" },
    TermKeyMap{ VK_F12, L"\x1b[24~" },
    // The numpad has a variety of mappings, none of which seem standard or really configurable by the OS.
    // See http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-PC-Style-Function-Keys
    //   to see just how convoluted this all is.
    // PuTTY uses a set of mappings that don't work in ViM without reamapping them back to the numpad
    // (see http://vim.wikia.com/wiki/PuTTY_numeric_keypad_mappings#Comments)
    // I think the best solution is to just not do any for the time being.
    // Putty also provides configuration for choosing which of the 5 mappings it has through the settings, which is more work than we can manage now.
    // TermKeyMap{ VK_MULTIPLY, L"\x1bOj" },     // PuTTY: \x1bOR (I believe putty is treating the top row of the numpad as PF1-PF4)
    // TermKeyMap{ VK_ADD, L"\x1bOk" },          // PuTTY: \x1bOl, \x1bOm (with shift)
    // TermKeyMap{ VK_SEPARATOR, L"\x1bOl" },    // ? I'm not sure which key this is...
    // TermKeyMap{ VK_SUBTRACT, L"\x1bOm" },     // \x1bOS
    // TermKeyMap{ VK_DECIMAL, L"\x1bOn" },      // \x1bOn
    // TermKeyMap{ VK_DIVIDE, L"\x1bOo" },       // \x1bOQ
    // TermKeyMap{ VK_NUMPAD0, L"\x1bOp" },
    // TermKeyMap{ VK_NUMPAD1, L"\x1bOq" },
    // TermKeyMap{ VK_NUMPAD2, L"\x1bOr" },
    // TermKeyMap{ VK_NUMPAD3, L"\x1bOs" },
    // TermKeyMap{ VK_NUMPAD4, L"\x1bOt" },
    // TermKeyMap{ VK_NUMPAD5, L"\x1bOu" }, // \x1b0E
    // TermKeyMap{ VK_NUMPAD5, L"\x1bOE" }, // PuTTY \x1b[G
    // TermKeyMap{ VK_NUMPAD6, L"\x1bOv" },
    // TermKeyMap{ VK_NUMPAD7, L"\x1bOw" },
    // TermKeyMap{ VK_NUMPAD8, L"\x1bOx" },
    // TermKeyMap{ VK_NUMPAD9, L"\x1bOy" },
    // TermKeyMap{ '=', L"\x1bOX" },      // I've also seen these codes mentioned in some documentation,
    // TermKeyMap{ VK_SPACE, L"\x1bO " }, //  but I wasn't really sure if they should be included or not...
    // TermKeyMap{ VK_TAB, L"\x1bOI" },   // So I left them here as a reference just in case.
};

// Sequences to send when a modifier is pressed with any of these keys
// Basically, the 'm' will be replaced with a character indicating which
//      modifier keys are pressed.
static constexpr std::array<TermKeyMap, 22> s_modifierKeyMapping{
    TermKeyMap{ VK_UP, L"\x1b[1;mA" },
    TermKeyMap{ VK_DOWN, L"\x1b[1;mB" },
    TermKeyMap{ VK_RIGHT, L"\x1b[1;mC" },
    TermKeyMap{ VK_LEFT, L"\x1b[1;mD" },
    TermKeyMap{ VK_HOME, L"\x1b[1;mH" },
    TermKeyMap{ VK_END, L"\x1b[1;mF" },
    TermKeyMap{ VK_F1, L"\x1b[1;mP" },
    TermKeyMap{ VK_F2, L"\x1b[1;mQ" },
    TermKeyMap{ VK_F3, L"\x1b[1;mR" },
    TermKeyMap{ VK_F4, L"\x1b[1;mS" },
    TermKeyMap{ VK_INSERT, L"\x1b[2;m~" },
    TermKeyMap{ VK_DELETE, L"\x1b[3;m~" },
    TermKeyMap{ VK_PRIOR, L"\x1b[5;m~" },
    TermKeyMap{ VK_NEXT, L"\x1b[6;m~" },
    TermKeyMap{ VK_F5, L"\x1b[15;m~" },
    TermKeyMap{ VK_F6, L"\x1b[17;m~" },
    TermKeyMap{ VK_F7, L"\x1b[18;m~" },
    TermKeyMap{ VK_F8, L"\x1b[19;m~" },
    TermKeyMap{ VK_F9, L"\x1b[20;m~" },
    TermKeyMap{ VK_F10, L"\x1b[21;m~" },
    TermKeyMap{ VK_F11, L"\x1b[23;m~" },
    TermKeyMap{ VK_F12, L"\x1b[24;m~" },
    // Ubuntu's inputrc also defines \x1b[5C, \x1b\x1bC (and D) as 'forward/backward-word' mappings
    // I believe '\x1b\x1bC' is listed because the C1 ESC (x9B) gets encoded as
    //  \xC2\x9B, but then translated to \x1b\x1b if the C1 codepoint isn't supported by the current encoding
};

// Sequences to send when a modifier is pressed with any of these keys
// These sequences are not later updated to encode the modifier state in the
//      sequence itself, they are just weird exceptional cases to the general
//      rules above.
static constexpr std::array<TermKeyMap, 6> s_simpleModifiedKeyMapping{
    TermKeyMap{ VK_BACK, CTRL_PRESSED, L"\x8" },
    TermKeyMap{ VK_BACK, ALT_PRESSED, L"\x1b\x7f" },
    TermKeyMap{ VK_BACK, CTRL_PRESSED | ALT_PRESSED, L"\x1b\x8" },
    TermKeyMap{ VK_TAB, CTRL_PRESSED, L"\t" },
    TermKeyMap{ VK_TAB, SHIFT_PRESSED, L"\x1b[Z" },
    TermKeyMap{ VK_DIVIDE, CTRL_PRESSED, L"\x1F" },
    // These two are not implemented here, because they are system keys.
    // TermKeyMap{ VK_TAB, ALT_PRESSED, L""}, This is the Windows system shortcut for switching windows.
    // TermKeyMap{ VK_ESCAPE, ALT_PRESSED, L""}, This is another Windows system shortcut for switching windows.
};

const wchar_t* const CTRL_SLASH_SEQUENCE = L"\x1f";

void TerminalInput::ChangeKeypadMode(const bool applicationMode) noexcept
{
    _keypadApplicationMode = applicationMode;
}

void TerminalInput::ChangeCursorKeysMode(const bool applicationMode) noexcept
{
    _cursorApplicationMode = applicationMode;
}

static const std::basic_string_view<TermKeyMap> _getKeyMapping(const KeyEvent& keyEvent,
                                                               const bool cursorApplicationMode,
                                                               const bool keypadApplicationMode) noexcept
{
    if (keyEvent.IsCursorKey())
    {
        if (cursorApplicationMode)
        {
            return { s_cursorKeysApplicationMapping.data(), s_cursorKeysApplicationMapping.size() };
        }
        else
        {
            return { s_cursorKeysNormalMapping.data(), s_cursorKeysNormalMapping.size() };
        }
    }
    else
    {
        if (keypadApplicationMode)
        {
            return { s_keypadApplicationMapping.data(), s_keypadApplicationMapping.size() };
        }
        else
        {
            return { s_keypadNumericMapping.data(), s_keypadNumericMapping.size() };
        }
    }
}

// Routine Description:
// - Searches the keyMapping for a entry corresponding to this key event, and returns it.
// Arguments:
// - keyEvent - Key event to translate
// - keyMapping - Array of key mappings to search
// Return Value:
// - Has value if there was a match to a key translation.
static std::optional<const TermKeyMap> _searchKeyMapping(const KeyEvent& keyEvent,
                                                         std::basic_string_view<TermKeyMap> keyMapping) noexcept
{
    for (auto& map : keyMapping)
    {
        if (map.vkey == keyEvent.GetVirtualKeyCode())
        {
            // If the mapping has no modifiers set, then it doesn't really care
            //      what the modifiers are on the key. The caller will likely do
            //      something with them.
            // However, if there are modifiers set, then we only want to match
            //      if the key's modifiers are the same as the modifiers in the
            //      mapping.
            bool modifiersMatch = WI_AreAllFlagsClear(map.modifiers, MOD_PRESSED);
            if (!modifiersMatch)
            {
                // The modifier mapping expects certain modifier keys to be
                //      pressed. Check those as well.
                modifiersMatch =
                    (WI_IsFlagSet(map.modifiers, SHIFT_PRESSED) == keyEvent.IsShiftPressed()) &&
                    (WI_IsAnyFlagSet(map.modifiers, ALT_PRESSED) == keyEvent.IsAltPressed()) &&
                    (WI_IsAnyFlagSet(map.modifiers, CTRL_PRESSED) == keyEvent.IsCtrlPressed());
            }

            if (modifiersMatch)
            {
                return map;
            }
        }
    }
    return std::nullopt;
}

typedef std::function<void(const std::wstring_view)> InputSender;

// Routine Description:
// - Searches the s_modifierKeyMapping for a entry corresponding to this key event.
//      Changes the second to last byte to correspond to the currently pressed modifier keys
//      before sending to the input.
// Arguments:
// - keyEvent - Key event to translate
// - sender - Function to use to dispatch translated event
// Return Value:
// - True if there was a match to a key translation, and we successfully modified and sent it to the input
static bool _searchWithModifier(const KeyEvent& keyEvent, InputSender sender)
{
    bool success = false;

    const auto match = _searchKeyMapping(keyEvent,
                                         { s_modifierKeyMapping.data(), s_modifierKeyMapping.size() });
    if (match)
    {
        const auto v = match.value();
        if (!v.sequence.empty())
        {
            std::wstring modified{ v.sequence }; // Make a copy so we can modify it.
            const bool shift = keyEvent.IsShiftPressed();
            const bool alt = keyEvent.IsAltPressed();
            const bool ctrl = keyEvent.IsCtrlPressed();
            modified.at(modified.size() - 2) = L'1' + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);
            sender(modified);
            success = true;
        }
    }
    else
    {
        // We didn't find the key in the map of modified keys that need editing,
        //      maybe it's in the other map of modified keys with sequences that
        //      don't need editing before sending.
        const auto match2 = _searchKeyMapping(keyEvent,
                                              { s_simpleModifiedKeyMapping.data(), s_simpleModifiedKeyMapping.size() });
        if (match2)
        {
            // This mapping doesn't need to be changed at all.
            sender(match2.value().sequence);
            success = true;
        }
        else
        {
            // One last check: C-/ is supposed to be C-_
            // But '/' is not the same VKEY on all keyboards. So we have to
            //      figure out the vkey at runtime.
            const BYTE slashVkey = LOBYTE(VkKeyScan(L'/'));
            if (keyEvent.GetVirtualKeyCode() == slashVkey && keyEvent.IsCtrlPressed())
            {
                // This mapping doesn't need to be changed at all.
                sender(CTRL_SLASH_SEQUENCE);
                success = true;
            }
        }
    }

    return success;
}

// Routine Description:
// - Searches the input array of mappings, and sends it to the input if a match was found.
// Arguments:
// - keyEvent - Key event to translate
// - keyMapping - Array of key mappings to search
// - sender - Function to use to dispatch translated event
// Return Value:
// - True if there was a match to a key translation, and we successfully sent it to the input
static bool _translateDefaultMapping(const KeyEvent& keyEvent,
                                     const std::basic_string_view<TermKeyMap> keyMapping,
                                     InputSender sender)
{
    const auto match = _searchKeyMapping(keyEvent, keyMapping);
    if (match)
    {
        sender(match->sequence);
    }
    return match.has_value();
}

bool TerminalInput::HandleKey(const IInputEvent* const pInEvent) const
{
    // By default, we fail to handle the key
    bool keyHandled = false;

    // On key presses, prepare to translate to VT compatible sequences
    if (pInEvent->EventType() == InputEventType::KeyEvent)
    {
        const auto senderFunc = [this](const std::wstring_view seq) { _SendInputSequence(seq); };

        auto keyEvent = *static_cast<const KeyEvent* const>(pInEvent);

        // Only need to handle key down. See raw key handler (see RawReadWaitRoutine in stream.cpp)
        if (keyEvent.IsKeyDown())
        {
            // For AltGr enabled keyboards, the Windows system will
            // emit Left Ctrl + Right Alt as the modifier keys and
            // will have pretranslated the UnicodeChar to the proper
            // alternative value.
            // Through testing with Ubuntu, PuTTY, and Emacs for
            // Windows, it was discovered that any instance of Left
            // Ctrl + Right Alt will strip out those two modifiers and
            // send the unicode value straight through to the system.
            // Holding additional modifiers in addition to Left Ctrl +
            // Right Alt will then light those modifiers up again for
            // the unicode value.
            // Therefore to handle AltGr properly, our first step
            // needs to be to check if both Left Ctrl + Right Alt are
            // pressed...
            // ... and if they are both pressed, strip them out of the control key state.
            if (keyEvent.IsAltGrPressed())
            {
                keyEvent.DeactivateModifierKey(ModifierKeyState::LeftCtrl);
                keyEvent.DeactivateModifierKey(ModifierKeyState::RightAlt);
            }

            if (keyEvent.IsAltPressed() &&
                keyEvent.IsCtrlPressed() &&
                (keyEvent.GetCharData() == 0 || keyEvent.GetCharData() == 0x20) &&
                ((keyEvent.GetVirtualKeyCode() > 0x40 && keyEvent.GetVirtualKeyCode() <= 0x5A) ||
                 keyEvent.GetVirtualKeyCode() == VK_SPACE))
            {
                // For Alt+Ctrl+Key messages, the UnicodeChar is NOT the Ctrl+key char, it's null.
                //      So we need to get the char from the vKey.
                //      EXCEPT for Alt+Ctrl+Space. Then the UnicodeChar is space, not NUL.
                auto wchPressedChar = gsl::narrow_cast<wchar_t>(MapVirtualKeyW(keyEvent.GetVirtualKeyCode(), MAPVK_VK_TO_CHAR));
                // This is a trick - C-Spc is supposed to send NUL. So quick change space -> @ (0x40)
                wchPressedChar = (wchPressedChar == UNICODE_SPACE) ? 0x40 : wchPressedChar;
                if (wchPressedChar >= 0x40 && wchPressedChar < 0x7F)
                {
                    //shift the char to the ctrl range
                    wchPressedChar -= 0x40;
                    _SendEscapedInputSequence(wchPressedChar);
                    keyHandled = true;
                }
            }

            // If a modifier key was pressed, then we need to try and send the modified sequence.
            if (!keyHandled && keyEvent.IsModifierPressed())
            {
                // Translate the key using the modifier table
                keyHandled = _searchWithModifier(keyEvent, senderFunc);
            }
            // ALT is a sequence of ESC + KEY.
            if (!keyHandled && keyEvent.GetCharData() != 0 && keyEvent.IsAltPressed())
            {
                _SendEscapedInputSequence(keyEvent.GetCharData());
                keyHandled = true;
            }
            if (!keyHandled && keyEvent.IsCtrlPressed())
            {
                if ((keyEvent.GetCharData() == UNICODE_SPACE) || // Ctrl+Space
                    // when Ctrl+@ comes through, the unicodechar
                    // will be '\x0' (UNICODE_NULL), and the vkey will be
                    // VkKeyScanW(0), the vkey for null
                    (keyEvent.GetCharData() == UNICODE_NULL && keyEvent.GetVirtualKeyCode() == LOBYTE(VkKeyScanW(0))))
                {
                    _SendNullInputSequence(keyEvent.GetActiveModifierKeys());
                    keyHandled = true;
                }
            }

            if (!keyHandled)
            {
                // For perf optimization, filter out any typically printable Virtual Keys (e.g. A-Z)
                // This is in lieu of an O(1) sparse table or other such less-maintainable methods.
                // VK_CANCEL is an exception and we want to send the associated uChar as is.
                if ((keyEvent.GetVirtualKeyCode() < '0' || keyEvent.GetVirtualKeyCode() > 'Z') &&
                    keyEvent.GetVirtualKeyCode() != VK_CANCEL)
                {
                    keyHandled = _translateDefaultMapping(keyEvent, _getKeyMapping(keyEvent, _cursorApplicationMode, _keypadApplicationMode), senderFunc);
                }
                else
                {
                    WCHAR rgwchSequence[2];
                    rgwchSequence[0] = keyEvent.GetCharData();
                    rgwchSequence[1] = UNICODE_NULL;
                    _SendInputSequence(rgwchSequence);
                    keyHandled = true;
                }
            }
        }
    }

    return keyHandled;
}

bool TerminalInput::HandleChar(const wchar_t ch)
{
    if (ch == UNICODE_BACKSPACE || ch == UNICODE_DEL)
    {
        return false;
    }
    else if (Utf16Parser::IsLeadingSurrogate(ch))
    {
        if (_leadingSurrogate.has_value())
        {
            // we already were storing a leading surrogate but we got another one. Go ahead and send the
            // saved surrogate piece and save the new one
            const auto formatted = wil::str_printf<std::wstring>(L"%I32u", _leadingSurrogate.value());
            _SendInputSequence(formatted);
        }
        // save the leading portion of a surrogate pair so that they can be sent at the same time
        _leadingSurrogate.emplace(ch);
    }
    else if (_leadingSurrogate.has_value())
    {
        std::wstring wstr;
        wstr.reserve(2);
        wstr.push_back(_leadingSurrogate.value());
        wstr.push_back(ch);
        _leadingSurrogate.reset();

        _SendInputSequence(wstr);
    }
    else
    {
        _SendInputSequence({ &ch, 1 });
    }

    return true;
}

// Routine Description:
// - Sends the given char as a sequence representing Alt+wch, also the same as
//      Meta+wch.
// Arguments:
// - wch - character to send to input paired with Esc
// Return Value:
// - None
void TerminalInput::_SendEscapedInputSequence(const wchar_t wch) const
{
    try
    {
        std::deque<std::unique_ptr<IInputEvent>> inputEvents;
        inputEvents.push_back(std::make_unique<KeyEvent>(true, 1ui16, 0ui16, 0ui16, L'\x1b', 0));
        inputEvents.push_back(std::make_unique<KeyEvent>(true, 1ui16, 0ui16, 0ui16, wch, 0));
        _pfnWriteEvents(inputEvents);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void TerminalInput::_SendNullInputSequence(const DWORD controlKeyState) const
{
    try
    {
        std::deque<std::unique_ptr<IInputEvent>> inputEvents;
        inputEvents.push_back(std::make_unique<KeyEvent>(true,
                                                         1ui16,
                                                         LOBYTE(VkKeyScanW(0)),
                                                         0ui16,
                                                         L'\x0',
                                                         controlKeyState));
        _pfnWriteEvents(inputEvents);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void TerminalInput::_SendInputSequence(const std::wstring_view sequence) const noexcept
{
    if (!sequence.empty())
    {
        try
        {
            std::deque<std::unique_ptr<IInputEvent>> inputEvents;
            for (const auto& wch : sequence)
            {
                inputEvents.push_back(std::make_unique<KeyEvent>(true, 1ui16, 0ui16, 0ui16, wch, 0));
            }
            _pfnWriteEvents(inputEvents);
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
        }
    }
}
