// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"

#include "strsafe.h"

#define WIL_SUPPORT_BITOPERATION_PASCAL_NAMES
#include <wil/Common.h>

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif

#include "../../inc/unicode.hpp"
#include "../../types/inc/Utf16Parser.hpp"

using namespace Microsoft::Console::VirtualTerminal;

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

static constexpr std::array<TermKeyMap, 6> s_cursorKeysVt52Mapping{
    TermKeyMap{ VK_UP, L"\033A" },
    TermKeyMap{ VK_DOWN, L"\033B" },
    TermKeyMap{ VK_RIGHT, L"\033C" },
    TermKeyMap{ VK_LEFT, L"\033D" },
    TermKeyMap{ VK_HOME, L"\033H" },
    TermKeyMap{ VK_END, L"\033F" },
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

static constexpr std::array<TermKeyMap, 20> s_keypadVt52Mapping{
    TermKeyMap{ VK_TAB, L"\x09" },
    TermKeyMap{ VK_BACK, L"\x7f" },
    TermKeyMap{ VK_PAUSE, L"\x1a" },
    TermKeyMap{ VK_ESCAPE, L"\x1b" },
    TermKeyMap{ VK_INSERT, L"\x1b[2~" },
    TermKeyMap{ VK_DELETE, L"\x1b[3~" },
    TermKeyMap{ VK_PRIOR, L"\x1b[5~" },
    TermKeyMap{ VK_NEXT, L"\x1b[6~" },
    TermKeyMap{ VK_F1, L"\x1bP" },
    TermKeyMap{ VK_F2, L"\x1bQ" },
    TermKeyMap{ VK_F3, L"\x1bR" },
    TermKeyMap{ VK_F4, L"\x1bS" },
    TermKeyMap{ VK_F5, L"\x1b[15~" },
    TermKeyMap{ VK_F6, L"\x1b[17~" },
    TermKeyMap{ VK_F7, L"\x1b[18~" },
    TermKeyMap{ VK_F8, L"\x1b[19~" },
    TermKeyMap{ VK_F9, L"\x1b[20~" },
    TermKeyMap{ VK_F10, L"\x1b[21~" },
    TermKeyMap{ VK_F11, L"\x1b[23~" },
    TermKeyMap{ VK_F12, L"\x1b[24~" },
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
static constexpr std::array<TermKeyMap, 14> s_simpleModifiedKeyMapping{
    TermKeyMap{ VK_BACK, CTRL_PRESSED, L"\x8" },
    TermKeyMap{ VK_BACK, ALT_PRESSED, L"\x1b\x7f" },
    TermKeyMap{ VK_BACK, CTRL_PRESSED | ALT_PRESSED, L"\x1b\x8" },
    TermKeyMap{ VK_TAB, CTRL_PRESSED, L"\t" },
    TermKeyMap{ VK_TAB, SHIFT_PRESSED, L"\x1b[Z" },
    TermKeyMap{ VK_DIVIDE, CTRL_PRESSED, L"\x1F" },

    // GH#3507 - We should also be encoding Ctrl+# according to the following table:
    // https://vt100.net/docs/vt220-rm/table3-5.html
    // * 1 and 9 do not send any special characters, but they _should_ send
    //   through the character unmodified.
    // * 0 doesn't seem to send even an unmodified '0' through.
    // * Ctrl+2 is already special-cased below in `HandleKey`, so it's not
    //   included here.
    TermKeyMap{ static_cast<WORD>('1'), CTRL_PRESSED, L"1" },
    // TermKeyMap{ static_cast<WORD>('2'), CTRL_PRESSED, L"\x00" },
    TermKeyMap{ static_cast<WORD>('3'), CTRL_PRESSED, L"\x1B" },
    TermKeyMap{ static_cast<WORD>('4'), CTRL_PRESSED, L"\x1C" },
    TermKeyMap{ static_cast<WORD>('5'), CTRL_PRESSED, L"\x1D" },
    TermKeyMap{ static_cast<WORD>('6'), CTRL_PRESSED, L"\x1E" },
    TermKeyMap{ static_cast<WORD>('7'), CTRL_PRESSED, L"\x1F" },
    TermKeyMap{ static_cast<WORD>('8'), CTRL_PRESSED, L"\x7F" },
    TermKeyMap{ static_cast<WORD>('9'), CTRL_PRESSED, L"9" },

    // These two are not implemented here, because they are system keys.
    // TermKeyMap{ VK_TAB, ALT_PRESSED, L""}, This is the Windows system shortcut for switching windows.
    // TermKeyMap{ VK_ESCAPE, ALT_PRESSED, L""}, This is another Windows system shortcut for switching windows.
};

const wchar_t* const CTRL_SLASH_SEQUENCE = L"\x1f";
const wchar_t* const CTRL_QUESTIONMARK_SEQUENCE = L"\x7F";
const wchar_t* const CTRL_ALT_SLASH_SEQUENCE = L"\x1b\x1f";
const wchar_t* const CTRL_ALT_QUESTIONMARK_SEQUENCE = L"\x1b\x7F";

void TerminalInput::ChangeAnsiMode(const bool ansiMode) noexcept
{
    _ansiMode = ansiMode;
}

void TerminalInput::ChangeKeypadMode(const bool applicationMode) noexcept
{
    _keypadApplicationMode = applicationMode;
}

void TerminalInput::ChangeCursorKeysMode(const bool applicationMode) noexcept
{
    _cursorApplicationMode = applicationMode;
}

void TerminalInput::ChangeWin32InputMode(const bool win32InputMode) noexcept
{
    _win32InputMode = win32InputMode;
}
void TerminalInput::ForceDisableWin32InputMode(const bool win32InputMode) noexcept
{
    _forceDisableWin32InputMode = win32InputMode;
}

static const gsl::span<const TermKeyMap> _getKeyMapping(const KeyEvent& keyEvent,
                                                        const bool ansiMode,
                                                        const bool cursorApplicationMode,
                                                        const bool keypadApplicationMode) noexcept
{
    if (ansiMode)
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
    else
    {
        if (keyEvent.IsCursorKey())
        {
            return { s_cursorKeysVt52Mapping.data(), s_cursorKeysVt52Mapping.size() };
        }
        else
        {
            return { s_keypadVt52Mapping.data(), s_keypadVt52Mapping.size() };
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
                                                         gsl::span<const TermKeyMap> keyMapping) noexcept
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
        const auto& v = match.value();
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
            // One last check:
            // * C-/ is supposed to be ^_ (the C0 character US)
            // * C-? is supposed to be DEL
            // * C-M-/ is supposed to be ^[^_
            // * C-M-? is supposed to be ^[^?
            //
            // But this whole scenario is tricky. '/' is not the same VKEY on
            // all keyboards. On USASCII keyboards, '/' and '?' share the _same_
            // key. So we have to figure out the vkey at runtime, and we have to
            // determine if the key that was pressed was '?' with some
            // modifiers, or '/' with some modifiers.
            //
            // These translations are not in s_simpleModifiedKeyMapping, because
            // the aforementioned fact that they aren't the same VKEY on all
            // keyboards.
            //
            // See GH#3079 for details.
            // Also see https://github.com/microsoft/terminal/pull/4947#issuecomment-600382856

            // VkKeyScan will give us both the Vkey of the key needed for this
            // character, and the modifiers the user might need to press to get
            // this character.
            const auto slashKeyScan = VkKeyScan(L'/'); // On USASCII: 0x00bf
            const auto questionMarkKeyScan = VkKeyScan(L'?'); //On USASCII: 0x01bf

            const auto slashVkey = LOBYTE(slashKeyScan);
            const auto questionMarkVkey = LOBYTE(questionMarkKeyScan);

            const auto ctrl = keyEvent.IsCtrlPressed();
            const auto alt = keyEvent.IsAltPressed();
            const bool shift = keyEvent.IsShiftPressed();

            // From the KeyEvent we're translating, synthesize the equivalent VkKeyScan result
            const auto vkey = keyEvent.GetVirtualKeyCode();
            const short keyScanFromEvent = vkey |
                                           (shift ? 0x100 : 0) |
                                           (ctrl ? 0x200 : 0) |
                                           (alt ? 0x400 : 0);

            // Make sure the VKEY is an _exact_ match, and that the modifier
            // bits also match. This handles the hypothetical case we get a
            // keyscan back that's ctrl+alt+some_random_VK, and some_random_VK
            // has bits that are a superset of the bits set for question mark.
            const bool wasQuestionMark = vkey == questionMarkVkey && WI_AreAllFlagsSet(keyScanFromEvent, questionMarkKeyScan);
            const bool wasSlash = vkey == slashVkey && WI_AreAllFlagsSet(keyScanFromEvent, slashKeyScan);

            // If the key pressed was exactly the ? key, then try to send the
            // appropriate sequence for a modified '?'. Otherwise, check if this
            // was a modified '/' keypress. These mappings don't need to be
            // changed at all.
            if ((ctrl && alt) && wasQuestionMark)
            {
                sender(CTRL_ALT_QUESTIONMARK_SEQUENCE);
                success = true;
            }
            else if (ctrl && wasQuestionMark)
            {
                sender(CTRL_QUESTIONMARK_SEQUENCE);
                success = true;
            }
            else if ((ctrl && alt) && wasSlash)
            {
                sender(CTRL_ALT_SLASH_SEQUENCE);
                success = true;
            }
            else if (ctrl && wasSlash)
            {
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
                                     const gsl::span<const TermKeyMap> keyMapping,
                                     InputSender sender)
{
    const auto match = _searchKeyMapping(keyEvent, keyMapping);
    if (match)
    {
        sender(match->sequence);
    }
    return match.has_value();
}

// Routine Description:
// - Sends the given input event to the shell.
// - The caller should attempt to fill the char data in pInEvent if possible.
//   The char data should already be translated in accordance to Ctrl/Alt/Shift
//   modifiers, like the characters given by the WM_CHAR event.
// - The caller doesn't need to fill in any char data for:
//   - Tab key
//   - Alt+key combinations
// - This method will alias Ctrl+Space as a synonym for Ctrl+@ - the null byte.
// Arguments:
// - keyEvent - Key event to translate
// Return Value:
// - True if the event was handled.
bool TerminalInput::HandleKey(const IInputEvent* const pInEvent)
{
    if (!pInEvent)
    {
        return false;
    }

    // On key presses, prepare to translate to VT compatible sequences
    if (pInEvent->EventType() != InputEventType::KeyEvent)
    {
        return false;
    }

    auto keyEvent = *static_cast<const KeyEvent* const>(pInEvent);

    // GH#4999 - If we're in win32-input mode, skip straight to doing that.
    // Since this mode handles all types of key events, do nothing else.
    // Only do this if win32-input-mode support isn't manually disabled.
    if (_win32InputMode && !_forceDisableWin32InputMode)
    {
        const auto seq = _GenerateWin32KeySequence(keyEvent);
        _SendInputSequence(seq);
        return true;
    }

    // Only need to handle key down. See raw key handler (see RawReadWaitRoutine in stream.cpp)
    if (!keyEvent.IsKeyDown())
    {
        return false;
    }

    // Many keyboard layouts have an AltGr key, which makes widely used characters accessible.
    // For instance on a German keyboard layout "[" is written by pressing AltGr+8.
    // Furthermore Ctrl+Alt is traditionally treated as an alternative way to AltGr by Windows.
    // When AltGr is pressed, the caller needs to make sure to send us a pretranslated character in GetCharData().
    // --> Strip out the AltGr flags, in order for us to not step into the Alt/Ctrl conditions below.
    if (keyEvent.IsAltGrPressed())
    {
        keyEvent.DeactivateModifierKey(ModifierKeyState::LeftCtrl);
        keyEvent.DeactivateModifierKey(ModifierKeyState::RightAlt);
    }

    // The Alt modifier initiates a so called "escape sequence".
    // See: https://en.wikipedia.org/wiki/ANSI_escape_code#Escape_sequences
    // See: ECMA-48, section 5.3, http://www.ecma-international.org/publications/standards/Ecma-048.htm
    //
    // This section in particular handles Alt+Ctrl combinations though.
    // The Ctrl modifier causes all of the char code's bits except
    // for the 5 least significant ones to be zeroed out.
    if (keyEvent.IsAltPressed() && keyEvent.IsCtrlPressed())
    {
        const auto ch = keyEvent.GetCharData();
        const auto vkey = keyEvent.GetVirtualKeyCode();

        // For Alt+Ctrl+Key messages GetCharData() usually returns 0.
        // Luckily the numerical values of the ASCII characters and virtual key codes
        // of <Space> and A-Z, as used below, are numerically identical.
        // -> Get the char from the virtual key if it's 0.
        const auto ctrlAltChar = keyEvent.GetCharData() != 0 ? keyEvent.GetCharData() : keyEvent.GetVirtualKeyCode();

        // Alt+Ctrl acts as a substitute for AltGr on Windows.
        // For instance using a German keyboard both AltGr+< and Alt+Ctrl+< produce a | (pipe) character.
        // The below condition primitively ensures that we allow all common Alt+Ctrl combinations
        // while preserving most of the functionality of Alt+Ctrl as a substitute for AltGr.
        if (ctrlAltChar == UNICODE_SPACE || (ctrlAltChar > 0x40 && ctrlAltChar <= 0x5A))
        {
            // Pressing the control key causes all bits but the 5 least
            // significant ones to be zeroed out (when using ASCII).
            _SendEscapedInputSequence(ctrlAltChar & 0b11111);
            return true;
        }

        // Currently, when we're called with Alt+Ctrl+@, ch will be 0, since Ctrl+@ equals a null byte.
        // VkKeyScanW(0) in turn returns the vkey for the null character (ASCII @).
        // -> Use the vkey to determine if Ctrl+@ is being pressed and produce ^[^@.
        if (ch == UNICODE_NULL && vkey == LOBYTE(VkKeyScanW(0)))
        {
            _SendEscapedInputSequence(L'\0');
            return true;
        }
    }

    const auto senderFunc = [this](const std::wstring_view seq) noexcept {
        _SendInputSequence(seq);
    };

    // If a modifier key was pressed, then we need to try and send the modified sequence.
    if (keyEvent.IsModifierPressed() && _searchWithModifier(keyEvent, senderFunc))
    {
        return true;
    }

    // This section is similar to the Alt modifier section above,
    // but handles cases without Ctrl modifiers.
    if (keyEvent.IsAltPressed() && !keyEvent.IsCtrlPressed() && keyEvent.GetCharData() != 0)
    {
        _SendEscapedInputSequence(keyEvent.GetCharData());
        return true;
    }

    // Pressing the control key causes all bits but the 5 least
    // significant ones to be zeroed out (when using ASCII).
    // This results in Ctrl+Space and Ctrl+@ being equal to a null byte.
    // Normally the C0 control code set only defines Ctrl+@,
    // but Ctrl+Space is also widely accepted by most terminals.
    // -> Send a "null input sequence" in that case.
    // We don't need to handle other kinds of Ctrl combinations,
    // as we rely on the caller to pretranslate those to characters for us.
    if (!keyEvent.IsAltPressed() && keyEvent.IsCtrlPressed())
    {
        const auto ch = keyEvent.GetCharData();
        const auto vkey = keyEvent.GetVirtualKeyCode();

        // Currently, when we're called with Ctrl+@, ch will be 0, since Ctrl+@ equals a null byte.
        // VkKeyScanW(0) in turn returns the vkey for the null character (ASCII @).
        // -> Use the vkey to alternatively determine if Ctrl+@ is being pressed.
        if (ch == UNICODE_SPACE || (ch == UNICODE_NULL && vkey == LOBYTE(VkKeyScanW(0))))
        {
            _SendNullInputSequence(keyEvent.GetActiveModifierKeys());
            return true;
        }

        // Not all keyboard layouts contain mappings for Ctrl-key combinations.
        // For instance the US one contains a mapping of Ctrl+\ to ^\,
        // but the UK extended layout doesn't, in which case ch is null.
        if (ch == UNICODE_NULL)
        {
            // -> Try to infer the character from the vkey.
            auto mappedChar = LOWORD(MapVirtualKeyW(keyEvent.GetVirtualKeyCode(), MAPVK_VK_TO_CHAR));
            if (mappedChar)
            {
                // Pressing the control key causes all bits but the 5 least
                // significant ones to be zeroed out (when using ASCII).
                mappedChar &= 0b11111;
                _SendChar(mappedChar);
                return true;
            }
        }
    }

    // Check any other key mappings (like those for the F1-F12 keys).
    // These mappings will kick in no matter which modifiers are pressed and as such
    // must be checked last, or otherwise we'd override more complex key combinations.
    const auto mapping = _getKeyMapping(keyEvent, _ansiMode, _cursorApplicationMode, _keypadApplicationMode);
    if (_translateDefaultMapping(keyEvent, mapping, senderFunc))
    {
        return true;
    }

    // If all else fails we can finally try to send the character itself if there is any.
    if (keyEvent.GetCharData() != 0)
    {
        _SendChar(keyEvent.GetCharData());
        return true;
    }

    return false;
}

// Routine Description:
// - Sends the given character to the shell.
// - Surrogate pairs are being aggregated by this function before being sent.
// Arguments:
// - ch: The UTF-16 character to send.
void TerminalInput::_SendChar(const wchar_t ch)
{
    if (Utf16Parser::IsLeadingSurrogate(ch))
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
        std::array<wchar_t, 2> wstr{ { _leadingSurrogate.value(), ch } };
        _leadingSurrogate.reset();
        _SendInputSequence({ wstr.data(), wstr.size() });
    }
    else
    {
        _SendInputSequence({ &ch, 1 });
    }
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

// Method Description:
// - Synthesize a win32-input-mode sequence for the given keyevent.
// Arguments:
// - key: the KeyEvent to serialize.
// Return Value:
// - the formatted string representation of this key
std::wstring TerminalInput::_GenerateWin32KeySequence(const KeyEvent& key)
{
    // Sequences are formatted as follows:
    //
    // ^[ [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    //
    //      Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.
    //      Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.
    //      Uc: the decimal value of UnicodeChar - for example, NUL is "0", LF is
    //          "10", the character 'A' is "65". If omitted, defaults to '0'.
    //      Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.
    //      Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.
    //      Rc: the value of wRepeatCount - any number. If omitted, defaults to '1'.
    return fmt::format(FMT_COMPILE(L"\x1b[{};{};{};{};{};{}_"),
                       key.GetVirtualKeyCode(),
                       key.GetVirtualScanCode(),
                       static_cast<int>(key.GetCharData()),
                       key.IsKeyDown() ? 1 : 0,
                       key.GetActiveModifierKeys(),
                       key.GetRepeatCount());
}
