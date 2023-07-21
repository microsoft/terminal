// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "echoDispatch.hpp"

using namespace Microsoft::Console::VirtualTerminal;

void EchoDispatch::Print(const wchar_t wchPrintable)
{
    wprintf(L"Print: %c (0x%x)\r\n", wchPrintable, wchPrintable);
}

void EchoDispatch::PrintString(const std::wstring_view string)
{
    const std::wstring nullTermString(string); // string_view not guaranteed null terminated, but wprintf needs it.
    wprintf(L"PrintString: \"%s\" (%zd chars)\r\n", nullTermString.data(), nullTermString.size());
}
