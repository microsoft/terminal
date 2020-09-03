/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PasteConverter.h

Abstract:
- Pre-process the text pasted (presumably from the clipboard).

--*/

#pragma once

#include <string>

namespace Microsoft::Console::Utils
{
    enum PasteFlags
    {
        CarriageReturnNewline = 1,
        FilterControlCodes = 2,
        Bracketed = 4
    };

    DEFINE_ENUM_FLAG_OPERATORS(PasteFlags)

    class PasteConverter
    {
    public:
        static std::wstring Convert(const std::wstring& wstr, PasteFlags flags);
    };
}
