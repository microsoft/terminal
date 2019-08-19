/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- clipboard.hpp

Abstract:
- This module is used for clipboard operations

Author(s):
- Michael Niksa (MiNiksa) 10-Apr-2014
- Paul Campbell (PaulCam) 10-Apr-2014

Revision History:
- From components of clipbrd.h/.c
--*/

#pragma once

#include "precomp.h"

#include "..\..\host\screenInfo.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class Clipboard
    {
    public:
        static Clipboard& Instance();

        void Copy(_In_ bool const fAlsoCopyHtml = false);
        void StringPaste(_In_reads_(cchData) PCWCHAR pwchData,
                         const size_t cchData);
        void Paste();

    private:
        std::deque<std::unique_ptr<IInputEvent>> TextToKeyEvents(_In_reads_(cchData) const wchar_t* const pData,
                                                                 const size_t cchData);

        void StoreSelectionToClipboard(_In_ bool const fAlsoCopyHtml);

        TextBuffer::TextAndColor RetrieveTextFromBuffer(const SCREEN_INFORMATION& screenInfo,
                                                        const bool lineSelection,
                                                        const std::vector<SMALL_RECT>& selectionRects);

        void CopyHTMLToClipboard(const TextBuffer::TextAndColor& rows);
        void CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, _In_ bool const fAlsoCopyHtml);

        bool FilterCharacterOnPaste(_Inout_ WCHAR* const pwch);

#ifdef UNIT_TESTING
        friend class ClipboardTests;
#endif
    };
}
