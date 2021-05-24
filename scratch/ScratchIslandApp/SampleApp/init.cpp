// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include <WilErrorReporting.h>

BOOL WINAPI DllMain(HINSTANCE /*hInstDll*/, DWORD /*reason*/, LPVOID /*reserved*/)
{
    return TRUE;
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"SampleApp/Resources")
