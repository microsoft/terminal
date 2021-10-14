// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "resource.h"
#include "../types/inc/User32Utils.hpp"
#include <WilErrorReporting.h>

// !! BODGY !!
// Manually use the resources from TerminalApp as our resources.
// The WindowsTerminal project doesn't actually build a Resources.resw file, but
// we still need to be able to localize strings for the tray icon menu. Anything
// you want localized for WindowsTerminal.exe should be stuck in
// ...\TerminalApp\Resources\en-US\Resources.resw
// #include <LibraryResources.h>
// UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"TerminalApp/Resources");

void TryRunAsContentProcess();

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    // If we _are_ a content process, then this function will call ExitThread(),
    // after spawning some COM threads to deal with inbound COM requests to the
    // ContentProcess object.
    TryRunAsContentProcess();
    // If we weren't a content process, then we'll just move on, and do the
    // normal WindowsTerminal thing.

    return 0;
}
