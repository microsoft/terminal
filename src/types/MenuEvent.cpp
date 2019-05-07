// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

MenuEvent::~MenuEvent()
{
}

INPUT_RECORD MenuEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = MENU_EVENT;
    record.Event.MenuEvent.dwCommandId = _commandId;
    return record;
}

InputEventType MenuEvent::EventType() const noexcept
{
    return InputEventType::MenuEvent;
}

void MenuEvent::SetCommandId(const UINT commandId) noexcept
{
    _commandId = commandId;
}
