// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "handle.h"
#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

LockConsoleGuard::LockConsoleGuard(std::unique_lock<til::recursive_ticket_lock>&& guard) noexcept :
    _guard{ std::move(guard) }
{
}

LockConsoleGuard::~LockConsoleGuard()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.GetCSRecursionCount() == 1)
    {
        ProcessCtrlEvents();
    }
}

LockConsoleGuard LockConsole()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return LockConsoleGuard{ gci.LockConsole() };
}
