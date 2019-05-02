// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

MouseEvent::~MouseEvent()
{
}

INPUT_RECORD MouseEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = MOUSE_EVENT;
    record.Event.MouseEvent.dwMousePosition = _position;
    record.Event.MouseEvent.dwButtonState = _buttonState;
    record.Event.MouseEvent.dwControlKeyState = _activeModifierKeys;
    record.Event.MouseEvent.dwEventFlags = _eventFlags;
    return record;
}

InputEventType MouseEvent::EventType() const noexcept
{
    return InputEventType::MouseEvent;
}

void MouseEvent::SetPosition(const COORD position) noexcept
{
    _position = position;
}

void MouseEvent::SetButtonState(const DWORD buttonState) noexcept
{
    _buttonState = buttonState;
}

void MouseEvent::SetActiveModifierKeys(const DWORD activeModifierKeys) noexcept
{
    _activeModifierKeys = activeModifierKeys;
}
void MouseEvent::SetEventFlags(const DWORD eventFlags) noexcept
{
    _eventFlags = eventFlags;
}
