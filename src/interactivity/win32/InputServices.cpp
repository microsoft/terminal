// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "InputServices.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity::Win32;

BOOL InputServices::TranslateCharsetInfo(DWORD* lpSrc, LPCHARSETINFO lpCs, DWORD dwFlags)
{
    return ::TranslateCharsetInfo(lpSrc, lpCs, dwFlags);
}
