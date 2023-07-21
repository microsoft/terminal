// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

WindowBufferSizeEvent::~WindowBufferSizeEvent() = default;

INPUT_RECORD WindowBufferSizeEvent::ToInputRecord() const noexcept
{
    INPUT_RECORD record{ 0 };
    record.EventType = WINDOW_BUFFER_SIZE_EVENT;
    record.Event.WindowBufferSizeEvent.dwSize.X = ::base::saturated_cast<short>(_size.width);
    record.Event.WindowBufferSizeEvent.dwSize.Y = ::base::saturated_cast<short>(_size.height);
    return record;
}

InputEventType WindowBufferSizeEvent::EventType() const noexcept
{
    return InputEventType::WindowBufferSizeEvent;
}

void WindowBufferSizeEvent::SetSize(const til::size size) noexcept
{
    _size = size;
}
