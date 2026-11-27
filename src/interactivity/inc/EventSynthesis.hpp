/*++
Copyright (c) Microsoft Corporation

Module Name:
- EventSynthesis.hpp

Abstract:
- Defined functions for converting strings/characters into events (for interactivity)
    Separated from types/convert.

Author:
- Dustin Howett (duhowett) 10-Feb-2021

--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    // A character as it would be typed on the keyboard: the key and modifiers that
    // produce it. It expands to the same 2 or 4 key events as SynthesizeKeyboardEvents,
    // but at 6 bytes it's a lot cheaper to store than the INPUT_RECORDs themselves.
    struct TypedChar
    {
        wchar_t wch;
        WORD scanCode;
        BYTE virtualKey;
        BYTE modifiers; // HIBYTE of VkKeyScanW: 1 = shift, 2 = ctrl, 4 = alt
    };

    // Returns false if the character has to be entered via SynthesizeNumpadEvents instead.
    bool CharToTypedChar(wchar_t wch, TypedChar& out);
    TypedChar KeyStateToTypedChar(wchar_t wch, short keyState);
    size_t TypedCharEventCount(const TypedChar& ch) noexcept;
    size_t TypedCharToKeyEvents(const TypedChar& ch, std::array<INPUT_RECORD, 4>& out) noexcept;

    void CharToKeyEvents(wchar_t wch, unsigned int codepage, InputEventQueue& out);
    void SynthesizeKeyboardEvents(wchar_t wch, short keyState, InputEventQueue& out);
    void SynthesizeNumpadEvents(wchar_t wch, unsigned int codepage, InputEventQueue& out);
}
