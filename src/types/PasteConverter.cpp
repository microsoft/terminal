// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/PasteConverter.h"

using namespace Microsoft::Console::Utils;

std::wstring PasteConverter::Convert(const std::wstring& wstr, PasteFlags flags)
{
    std::wstring converted{ wstr };

    // Convert Windows-space \r\n line-endings to \r (carriage return) line-endings
    if (flags & PasteFlags::CarriageReturnNewline)
    {
        // Some notes on this implementation:
        //
        // - std::regex can do this in a single line, but is somewhat
        //   overkill for a simple search/replace operation (and its
        //   performance guarantees aren't exactly stellar)
        // - The STL doesn't have a simple string search/replace method.
        //   This fact is lamentable.
        // - This line-ending conversion is intentionally fairly
        //   conservative, to avoid stripping out lone \n characters
        //   where they could conceivably be intentional.
        std::wstring::size_type pos = 0;

        while ((pos = converted.find(L"\r\n", pos)) != std::wstring::npos)
        {
            converted.replace(pos, 2, L"\r");
        }
    }

    if (flags & PasteFlags::FilterControlCodes)
    {
        // For security reasons, control characters should be filtered.
        // Here ASCII control characters will be removed, except HT(0x09), LF(0x0a), CR(0x0d) and DEL(0x7F).
        converted.erase(std::remove_if(converted.begin(), converted.end(), [](wchar_t c) {
            if (c >= L'\x20' && c <= L'\x7f')
            {
                // Printable ASCII + DEL.
                return false;
            }

            if (c > L'\x7f')
            {
                // Not a control character for sure.
                return false;
            }

            return c >= L'\x00' && c <= L'\x08' ||
                   c >= L'\x0b' && c <= L'\x0c' ||
                   c >= L'\x0e' && c <= L'\x1f';
        }));
    }

    // Bracketed Paste Mode, invented by xterm and implemented in many popular terminal emulators.
    // See: http://www.xfree86.org/current/ctlseqs.html#Bracketed%20Paste%20Mode
    if (flags & PasteFlags::Bracketed)
    {
        converted.insert(0, L"\x1b[200~");
        converted.append(L"\x1b[201~");
    }

    return converted;
}
