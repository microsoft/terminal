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
#include <deque>
#include <memory>
#include "../../types/inc/IInputEvent.hpp"

namespace Microsoft::Console::Interactivity
{
    std::deque<std::unique_ptr<KeyEvent>> CharToKeyEvents(const wchar_t wch, const unsigned int codepage);

    std::deque<std::unique_ptr<KeyEvent>> SynthesizeKeyboardEvents(const wchar_t wch,
                                                                   const short keyState);

    std::deque<std::unique_ptr<KeyEvent>> SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage);
}
