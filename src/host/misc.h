/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- misc.h

Abstract:
- This file implements the NT console server font routines.

Author:
- Therese Stowell (ThereseS) 22-Jan-1991

Revision History:
 - Mike Griese, 30-oct-2017: Moved all functions that didn't require the host
    to the contypes lib. The ones that are still here in one way or another
    require code from the host to build.
--*/

#pragma once

#include "screenInfo.hpp"
#include "../types/inc/IInputEvent.hpp"
#include <deque>
#include <memory>

WCHAR CharToWchar(_In_reads_(cch) const char* const pch, const UINT cch);

void SetConsoleCPInfo(const BOOL fOutput);

BOOL CheckBisectStringW(_In_reads_bytes_(cBytes) const WCHAR* pwchBuffer,
                        _In_ size_t cWords,
                        _In_ size_t cBytes) noexcept;
BOOL CheckBisectProcessW(const SCREEN_INFORMATION& ScreenInfo,
                         _In_reads_bytes_(cBytes) const WCHAR* pwchBuffer,
                         _In_ size_t cWords,
                         _In_ size_t cBytes,
                         _In_ til::CoordType sOriginalXPosition,
                         _In_ BOOL fPrintableControlChars);

int ConvertToOem(const UINT uiCodePage,
                 _In_reads_(cchSource) const WCHAR* const pwchSource,
                 const UINT cchSource,
                 _Out_writes_(cchTarget) CHAR* const pchTarget,
                 const UINT cchTarget) noexcept;

template<typename CharT, typename Traits>
void StringToInputEvents(std::basic_string_view<CharT, Traits> str, std::deque<std::unique_ptr<IInputEvent>>& events)
{
    for (const auto& ch : str)
    {
        auto event = std::make_unique<KeyEvent>();
        event->SetKeyDown(true);
        event->SetRepeatCount(1);
        event->SetCharData(ch);
        events.push_back(std::move(event));
    }
}

template<typename CharT, typename Traits>
void InputEventsToString(std::deque<std::unique_ptr<IInputEvent>>& events, std::basic_string<CharT, Traits>& str)
{
    for (const auto& event : events)
    {
        if (event->EventType() == InputEventType::KeyEvent)
        {
            const auto keyEvent = static_cast<const KeyEvent*>(event.get());
            // This keydown check prevents us from serializing ABC keypresses
            // into AABBCC because each key-down is followed by a key-up.
            if (keyEvent->IsKeyDown())
            {
                const auto wch = keyEvent->GetCharData();
                str.push_back(static_cast<CharT>(wch));
            }
        }
    }
}

void SplitToOem(std::deque<std::unique_ptr<IInputEvent>>& events);

int ConvertInputToUnicode(const UINT uiCodePage,
                          _In_reads_(cchSource) const CHAR* const pchSource,
                          const UINT cchSource,
                          _Out_writes_(cchTarget) WCHAR* const pwchTarget,
                          const UINT cchTarget) noexcept;

int ConvertOutputToUnicode(_In_ UINT uiCodePage,
                           _In_reads_(cchSource) const CHAR* const pchSource,
                           _In_ UINT cchSource,
                           _Out_writes_(cchTarget) WCHAR* pwchTarget,
                           _In_ UINT cchTarget) noexcept;

bool DoBuffersOverlap(const BYTE* const pBufferA,
                      const UINT cbBufferA,
                      const BYTE* const pBufferB,
                      const UINT cbBufferB) noexcept;
