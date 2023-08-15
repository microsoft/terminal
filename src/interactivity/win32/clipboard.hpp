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

#include "../../host/screenInfo.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class Clipboard
    {
    public:
        static Clipboard& Instance();

        void Copy(_In_ const bool fAlsoCopyFormatting = false);
        void StringPaste(_In_reads_(cchData) PCWCHAR pwchData,
                         const size_t cchData);
        void Paste();

    private:
        InputEventQueue TextToKeyEvents(_In_reads_(cchData) const wchar_t* const pData,
                                        const size_t cchData,
                                        const bool bracketedPaste = false);

        void StoreSelectionToClipboard(_In_ const bool fAlsoCopyFormatting);

        void CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, _In_ const bool copyFormatting);
        void CopyToSystemClipboard(std::string stringToPlaceOnClip, LPCWSTR lpszFormat);

        bool FilterCharacterOnPaste(_Inout_ WCHAR* const pwch);

#ifdef UNIT_TESTING
        friend class ClipboardTests;
#endif
    };
}
