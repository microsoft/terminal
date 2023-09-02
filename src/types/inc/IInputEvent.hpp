// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/small_vector.h>

#define ALT_PRESSED (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)
#define CTRL_PRESSED (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)
#define MOD_PRESSED (SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED)

using InputEventQueue = til::small_vector<INPUT_RECORD, 16>;

// The following abstractions exist to hopefully make it easier to migrate
// to a different underlying InputEvent type if we ever choose to do so.
// (As unlikely as that is, given that the main user is the console API which will always use INPUT_RECORD.)
constexpr INPUT_RECORD SynthesizeKeyEvent(bool bKeyDown, uint16_t wRepeatCount, uint16_t wVirtualKeyCode, uint16_t wVirtualScanCode, wchar_t UnicodeChar, uint32_t dwControlKeyState)
{
    return INPUT_RECORD{
        .EventType = KEY_EVENT,
        .Event = {
            .KeyEvent = {
                .bKeyDown = bKeyDown,
                .wRepeatCount = wRepeatCount,
                .wVirtualKeyCode = wVirtualKeyCode,
                .wVirtualScanCode = wVirtualScanCode,
                .uChar = { .UnicodeChar = UnicodeChar },
                .dwControlKeyState = dwControlKeyState,
            },
        },
    };
}

constexpr INPUT_RECORD SynthesizeMouseEvent(til::point dwMousePosition, uint32_t dwButtonState, uint32_t dwControlKeyState, uint32_t dwEventFlags)
{
    return INPUT_RECORD{
        .EventType = MOUSE_EVENT,
        .Event = {
            .MouseEvent = {
                .dwMousePosition = {
                    ::base::saturated_cast<SHORT>(dwMousePosition.x),
                    ::base::saturated_cast<SHORT>(dwMousePosition.y),
                },
                .dwButtonState = dwButtonState,
                .dwControlKeyState = dwControlKeyState,
                .dwEventFlags = dwEventFlags,
            },
        },
    };
}

constexpr INPUT_RECORD SynthesizeWindowBufferSizeEvent(til::size dwSize)
{
    return INPUT_RECORD{
        .EventType = WINDOW_BUFFER_SIZE_EVENT,
        .Event = {
            .WindowBufferSizeEvent = {
                .dwSize = {
                    ::base::saturated_cast<SHORT>(dwSize.width),
                    ::base::saturated_cast<SHORT>(dwSize.height),
                },
            },
        },
    };
}

constexpr INPUT_RECORD SynthesizeMenuEvent(uint32_t dwCommandId)
{
    return INPUT_RECORD{
        .EventType = MENU_EVENT,
        .Event = {
            .MenuEvent = {
                .dwCommandId = dwCommandId,
            },
        },
    };
}

constexpr INPUT_RECORD SynthesizeFocusEvent(bool bSetFocus)
{
    return INPUT_RECORD{
        .EventType = FOCUS_EVENT,
        .Event = {
            .FocusEvent = {
                .bSetFocus = bSetFocus,
            },
        },
    };
}
