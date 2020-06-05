// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

KeyEvent::~KeyEvent()
{
}

INPUT_RECORD KeyEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = !!_keyDown;
    record.Event.KeyEvent.wRepeatCount = _repeatCount;
    record.Event.KeyEvent.wVirtualKeyCode = _virtualKeyCode;
    record.Event.KeyEvent.wVirtualScanCode = _virtualScanCode;
    record.Event.KeyEvent.uChar.UnicodeChar = _charData;
    record.Event.KeyEvent.dwControlKeyState = GetActiveModifierKeys();
    return record;
}

InputEventType KeyEvent::EventType() const noexcept
{
    return InputEventType::KeyEvent;
}

void KeyEvent::SetKeyDown(const bool keyDown) noexcept
{
    _keyDown = keyDown;
}

void KeyEvent::SetRepeatCount(const WORD repeatCount) noexcept
{
    _repeatCount = repeatCount;
}

void KeyEvent::SetVirtualKeyCode(const WORD virtualKeyCode) noexcept
{
    _virtualKeyCode = virtualKeyCode;
}

void KeyEvent::SetVirtualScanCode(const WORD virtualScanCode) noexcept
{
    _virtualScanCode = virtualScanCode;
}

void KeyEvent::SetCharData(const wchar_t character) noexcept
{
    _charData = character;
}

void KeyEvent::SetActiveModifierKeys(const DWORD activeModifierKeys) noexcept
{
    _activeModifierKeys = static_cast<KeyEvent::Modifiers>(activeModifierKeys);
}

void KeyEvent::DeactivateModifierKey(const ModifierKeyState modifierKey) noexcept
{
    DWORD const bitFlag = ToConsoleControlKeyFlag(modifierKey);
    auto keys = GetActiveModifierKeys();
    WI_ClearAllFlags(keys, bitFlag);
    SetActiveModifierKeys(keys);
}

void KeyEvent::ActivateModifierKey(const ModifierKeyState modifierKey) noexcept
{
    DWORD const bitFlag = ToConsoleControlKeyFlag(modifierKey);
    auto keys = GetActiveModifierKeys();
    WI_SetAllFlags(keys, bitFlag);
    SetActiveModifierKeys(keys);
}

bool KeyEvent::DoActiveModifierKeysMatch(const std::unordered_set<ModifierKeyState>& consoleModifiers) const
{
    DWORD consoleBits = 0;
    for (const ModifierKeyState& mod : consoleModifiers)
    {
        WI_SetAllFlags(consoleBits, ToConsoleControlKeyFlag(mod));
    }
    return consoleBits == GetActiveModifierKeys();
}

// Routine Description:
// - checks if this key event is a special key for line editing
// Arguments:
// - none
// Return Value:
// - true if this key has special relevance to line editing, false otherwise
bool KeyEvent::IsCommandLineEditingKey() const noexcept
{
    if (!IsAltPressed() && !IsCtrlPressed())
    {
        switch (GetVirtualKeyCode())
        {
        case VK_ESCAPE:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
            return true;
        default:
            break;
        }
    }
    if (IsCtrlPressed())
    {
        switch (GetVirtualKeyCode())
        {
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_RIGHT:
            return true;
        default:
            break;
        }
    }

    if (IsAltPressed())
    {
        switch (GetVirtualKeyCode())
        {
        case VK_F7:
        case VK_F10:
            return true;
        default:
            break;
        }
    }
    return false;
}

// Routine Description:
// - checks if this key event is a special key for popups
// Arguments:
// - None
// Return Value:
// - true if this key has special relevance to popups, false otherwise
bool KeyEvent::IsPopupKey() const noexcept
{
    if (!IsAltPressed() && !IsCtrlPressed())
    {
        switch (GetVirtualKeyCode())
        {
        case VK_ESCAPE:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_F2:
        case VK_F4:
        case VK_F7:
        case VK_F9:
        case VK_DELETE:
            return true;
        default:
            break;
        }
    }

    return false;
}
