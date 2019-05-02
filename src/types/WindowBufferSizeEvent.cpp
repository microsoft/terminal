// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

WindowBufferSizeEvent::~WindowBufferSizeEvent()
{
}

INPUT_RECORD WindowBufferSizeEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = WINDOW_BUFFER_SIZE_EVENT;
    record.Event.WindowBufferSizeEvent.dwSize = _size;
    return record;
}

InputEventType WindowBufferSizeEvent::EventType() const noexcept
{
    return InputEventType::WindowBufferSizeEvent;
}

void WindowBufferSizeEvent::SetSize(const COORD size) noexcept
{
    _size = size;
}
