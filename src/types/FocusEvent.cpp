// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// BODGY: GH#13238
//
// It appears that some applications (libuv) like to send a FOCUS_EVENT_RECORD
// as a way to jiggle the input handle. Focus events really shouldn't ever be
// sent via the API, they don't really do anything internally. However, focus
// events in the input buffer do get translated by the TerminalInput to VT
// sequences if we're in the right input mode.
//
// To not prevent libuv from jiggling the handle with a focus event, and also
// make sure that we don't erroneously translate that to a sequence of
// characters, we're going to filter out focus events that came from the API
// when translating to VT.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

FocusEvent::~FocusEvent() = default;

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
