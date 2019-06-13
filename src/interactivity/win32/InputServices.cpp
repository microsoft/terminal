// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "InputServices.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity::Win32;

UINT InputServices::MapVirtualKeyW(UINT uCode, UINT uMapType)
{
    return ::MapVirtualKeyW(uCode, uMapType);
}

SHORT InputServices::VkKeyScanW(WCHAR ch)
{
    return ::VkKeyScanW(ch);
}

SHORT InputServices::GetKeyState(int nVirtKey)
{
    return ::GetKeyState(nVirtKey);
}

BOOL InputServices::TranslateCharsetInfo(DWORD* lpSrc, LPCHARSETINFO lpCs, DWORD dwFlags)
{
    return ::TranslateCharsetInfo(lpSrc, lpCs, dwFlags);
}
