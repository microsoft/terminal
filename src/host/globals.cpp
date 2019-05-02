// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "globals.h"

#pragma hdrstop

CONSOLE_INFORMATION& Globals::getConsoleInformation()
{
    return ciConsoleInformation;
}

bool Globals::IsHeadless() const
{
    return launchArgs.IsHeadless();
}
