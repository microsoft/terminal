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

        void CopyText(const std::wstring& text);
        void Copy(_In_ const bool fAlsoCopyFormatting = false);
        void Paste();
        void PasteDrop(HDROP drop);

    private:
        static wil::unique_close_clipboard_call _openClipboard(HWND hwnd);
        static void _copyToClipboard(UINT format, const void* src, size_t bytes);
        static void _copyToClipboardRegisteredFormat(const wchar_t* format, const void* src, size_t bytes);

        void StringPaste(_In_reads_(cchData) PCWCHAR pwchData, const size_t cchData);
        InputEventQueue TextToKeyEvents(_In_reads_(cchData) const wchar_t* const pData,
                                        const size_t cchData,
                                        const bool bracketedPaste = false);

        void StoreSelectionToClipboard(_In_ const bool fAlsoCopyFormatting);

        bool FilterCharacterOnPaste(_Inout_ WCHAR* const pwch);

#ifdef UNIT_TESTING
        friend class ClipboardTests;
#endif
    };
}
