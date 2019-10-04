// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

HINSTANCE g_hInstance = nullptr;
HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hInstance,
                      DWORD,
                      LPVOID) noexcept
{
    g_hModule = g_hInstance = hInstance;
    return TRUE;
}
