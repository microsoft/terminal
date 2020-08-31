// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DelegationConfig.hpp"

#include "RegistrySerialization.hpp"

#pragma hdrstop

[[nodiscard]] HRESULT DelegationConfig::s_Get(IID& iid)
{
    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenConsoleKey(&currentUserKey, &consoleKey));

    wil::unique_hkey startupKey;
    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenKey(consoleKey.get(), L"%%Startup", &startupKey));

    DWORD bytesNeeded = 0;
    NTSTATUS result = RegistrySerialization::s_QueryValue(startupKey.get(),
                                                          L"DelegationConsole",
                                                          0,
                                                          REG_SZ,
                                                          nullptr,
                                                          &bytesNeeded);

    if (NTSTATUS_FROM_WIN32(ERROR_SUCCESS) != result)
    {
        RETURN_NTSTATUS(result);
    }

    auto buffer = std::make_unique<wchar_t[]>(bytesNeeded / sizeof(wchar_t));

    DWORD bytesUsed = 0;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_QueryValue(startupKey.get(),
                                                                  L"DelegationConsole",
                                                                  bytesNeeded,
                                                                  REG_SZ,
                                                                  reinterpret_cast<BYTE*>(buffer.get()),
                                                                  &bytesUsed));

    RETURN_IF_FAILED(IIDFromString(buffer.get(), &iid));

    return S_OK;
}
