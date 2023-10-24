// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "handle.h"
#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

void LockConsole()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
}

void UnlockConsole()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.GetCSRecursionCount() == 1)
    {
        ProcessCtrlEvents();
    }
    else
    {
        gci.UnlockConsole();
    }
}
