// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

FocusEvent::~FocusEvent()
{
}

INPUT_RECORD FocusEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = FOCUS_EVENT;
    record.Event.FocusEvent.bSetFocus = !!_focus;
    return record;
}

InputEventType FocusEvent::EventType() const noexcept
{
    return InputEventType::FocusEvent;
}

void FocusEvent::SetFocus(const bool focus) noexcept
{
    _focus = focus;
}
