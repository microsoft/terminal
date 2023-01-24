// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/VtApiRedirection.hpp"
#include "../onecore/ConIoSrvComm.hpp"
#pragma hdrstop

// The project include file defines these to be invalid symbols
// to clue in developers across the project not to use them.
//
// We have to use them here.
#undef VkKeyScanW
#undef MapVirtualKeyW
#undef GetKeyState

UINT OneCoreSafeMapVirtualKeyW(_In_ UINT uCode, _In_ UINT uMapType)
{
    auto ret{ MapVirtualKeyW(uCode, uMapType) };
#ifdef BUILD_ONECORE_INTERACTIVITY
    if (ret == 0)
    {
        const auto lastError{ GetLastError() };
        if (lastError == ERROR_PROC_NOT_FOUND || lastError == ERROR_DELAY_LOAD_FAILED)
        {
            if (auto conIoSrvComm{ Microsoft::Console::Interactivity::OneCore::ConIoSrvComm::GetConIoSrvComm() })
            {
                SetLastError(0);
                ret = conIoSrvComm->ConIoMapVirtualKeyW(uCode, uMapType);
            }
        }
    }
#endif
    return ret;
}

SHORT OneCoreSafeVkKeyScanW(_In_ WCHAR ch)
{
    auto ret{ VkKeyScanW(ch) };
#ifdef BUILD_ONECORE_INTERACTIVITY
    if (ret == -1)
    {
        const auto lastError{ GetLastError() };
        if (lastError == ERROR_PROC_NOT_FOUND || lastError == ERROR_DELAY_LOAD_FAILED)
        {
            if (auto conIoSrvComm{ Microsoft::Console::Interactivity::OneCore::ConIoSrvComm::GetConIoSrvComm() })
            {
                SetLastError(0);
                ret = conIoSrvComm->ConIoVkKeyScanW(ch);
            }
        }
    }
#endif
    return ret;
}

SHORT OneCoreSafeGetKeyState(_In_ int nVirtKey)
{
    auto ret{ GetKeyState(nVirtKey) };
#ifdef BUILD_ONECORE_INTERACTIVITY
    if (ret == 0)
    {
        const auto lastError{ GetLastError() };
        if (lastError == ERROR_PROC_NOT_FOUND || lastError == ERROR_DELAY_LOAD_FAILED)
        {
            if (auto conIoSrvComm{ Microsoft::Console::Interactivity::OneCore::ConIoSrvComm::GetConIoSrvComm() })
            {
                SetLastError(0);
                ret = conIoSrvComm->ConIoGetKeyState(nVirtKey);
            }
        }
    }
#endif
    return ret;
}
