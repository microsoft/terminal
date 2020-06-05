// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"
#include <string>

std::wostream& operator<<(std::wostream& stream, const IInputEvent* const pEvent)
{
    if (pEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    try
    {
        switch (pEvent->EventType())
        {
        case InputEventType::KeyEvent:
            return stream << static_cast<const KeyEvent* const>(pEvent);
        case InputEventType::MouseEvent:
            return stream << static_cast<const MouseEvent* const>(pEvent);
        case InputEventType::WindowBufferSizeEvent:
            return stream << static_cast<const WindowBufferSizeEvent* const>(pEvent);
        case InputEventType::MenuEvent:
            return stream << static_cast<const MenuEvent* const>(pEvent);
        case InputEventType::FocusEvent:
            return stream << static_cast<const FocusEvent* const>(pEvent);
        default:
            return stream << L"IInputEvent()";
        }
    }
    catch (...)
    {
        return stream << L"IInputEvent stream error";
    }
}

std::wostream& operator<<(std::wostream& stream, const KeyEvent* const pKeyEvent)
{
    if (pKeyEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    std::wstring keyMotion = pKeyEvent->_keyDown ? L"keyDown" : L"keyUp";
    std::wstring charData = { pKeyEvent->_charData };
    if (pKeyEvent->_charData == L'\0')
    {
        charData = L"null";
    }

    // clang-format off
    return stream << L"KeyEvent(" <<
        keyMotion << L", " <<
        L"repeat: " << pKeyEvent->_repeatCount << L", " <<
        L"keyCode: " << pKeyEvent->_virtualKeyCode << L", " <<
        L"scanCode: " << pKeyEvent->_virtualScanCode << L", " <<
        L"char: " << charData << L", " <<
        L"mods: " << pKeyEvent->GetActiveModifierKeys() << L")";
    // clang-format on
}

std::wostream& operator<<(std::wostream& stream, const MouseEvent* const pMouseEvent)
{
    if (pMouseEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    // clang-format off
    return stream << L"MouseEvent(" <<
        L"X: " << pMouseEvent->_position.X << L", " <<
        L"Y: " << pMouseEvent->_position.Y << L", " <<
        L"buttons: " << pMouseEvent->_buttonState << L", " <<
        L"mods: " << pMouseEvent->_activeModifierKeys << L", " <<
        L"events: " << pMouseEvent->_eventFlags << L")";
    // clang-format on
}

std::wostream& operator<<(std::wostream& stream, const WindowBufferSizeEvent* const pEvent)
{
    if (pEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    // clang-format off
    return stream << L"WindowbufferSizeEvent(" <<
        L"X: " << pEvent->_size.X << L", " <<
        L"Y: " << pEvent->_size.Y << L")";
    // clang-format on
}

std::wostream& operator<<(std::wostream& stream, const MenuEvent* const pMenuEvent)
{
    if (pMenuEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    return stream << L"MenuEvent(" << L"CommandId" << pMenuEvent->_commandId << L")";
}

std::wostream& operator<<(std::wostream& stream, const FocusEvent* const pFocusEvent)
{
    if (pFocusEvent == nullptr)
    {
        return stream << L"nullptr";
    }

    return stream << L"FocusEvent(" << L"focus" << pFocusEvent->_focus << L")";
}
