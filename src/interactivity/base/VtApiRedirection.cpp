// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\inc\VtApiRedirection.hpp"

#include "..\inc\ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity;

UINT VTRedirMapVirtualKeyW(_In_ UINT uCode, _In_ UINT uMapType)
{
    return ServiceLocator::LocateInputServices()->MapVirtualKeyW(uCode, uMapType);
}

SHORT VTRedirVkKeyScanW(_In_ WCHAR ch)
{
    return ServiceLocator::LocateInputServices()->VkKeyScanW(ch);
}

SHORT VTRedirGetKeyState(_In_ int nVirtKey)
{
    return ServiceLocator::LocateInputServices()->GetKeyState(nVirtKey);
}
