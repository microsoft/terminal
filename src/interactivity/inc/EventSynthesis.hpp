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
    void CharToKeyEvents(wchar_t wch, unsigned int codepage, InputEventQueue& out);
    void SynthesizeKeyboardEvents(wchar_t wch, short keyState, InputEventQueue& out);
    void SynthesizeNumpadEvents(wchar_t wch, unsigned int codepage, InputEventQueue& out);
}
