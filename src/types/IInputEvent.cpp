// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

#include <unordered_map>

std::unique_ptr<IInputEvent> IInputEvent::Create(const INPUT_RECORD& record)
{
    switch (record.EventType)
    {
    case KEY_EVENT:
        return std::make_unique<KeyEvent>(record.Event.KeyEvent);
    case MOUSE_EVENT:
        return std::make_unique<MouseEvent>(record.Event.MouseEvent);
    case WINDOW_BUFFER_SIZE_EVENT:
        return std::make_unique<WindowBufferSizeEvent>(record.Event.WindowBufferSizeEvent);
    case MENU_EVENT:
        return std::make_unique<MenuEvent>(record.Event.MenuEvent);
    case FOCUS_EVENT:
        return std::make_unique<FocusEvent>(record.Event.FocusEvent);
    default:
        THROW_HR(E_INVALIDARG);
    }
}

std::deque<std::unique_ptr<IInputEvent>> IInputEvent::Create(gsl::span<const INPUT_RECORD> records)
{
    std::deque<std::unique_ptr<IInputEvent>> outEvents;

    for (auto& record : records)
    {
        outEvents.push_back(Create(record));
    }

    return outEvents;
}

// Routine Description:
// - Converts std::deque<INPUT_RECORD> to std::deque<std::unique_ptr<IInputEvent>>
// Arguments:
// - inRecords - records to convert
// Return Value:
// - std::deque of IInputEvents on success. Will throw exception on failure.
std::deque<std::unique_ptr<IInputEvent>> IInputEvent::Create(const std::deque<INPUT_RECORD>& records)
{
    std::deque<std::unique_ptr<IInputEvent>> outEvents;
    for (size_t i = 0; i < records.size(); ++i)
    {
        std::unique_ptr<IInputEvent> event = IInputEvent::Create(records.at(i));
        outEvents.push_back(std::move(event));
    }
    return outEvents;
}

std::vector<INPUT_RECORD> IInputEvent::ToInputRecords(const std::deque<std::unique_ptr<IInputEvent>>& events)
{
    std::vector<INPUT_RECORD> records;
    records.reserve(events.size());

    for (auto& evt : events)
    {
        records.push_back(evt->ToInputRecord());
    }

    return records;
}
