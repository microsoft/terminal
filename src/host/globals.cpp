// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "globals.h"

// MidiAudio
#include <mmeapi.h>
#include <dsound.h>

#pragma hdrstop

Globals::Globals()
{
    api = &defaultApiRoutines;
}

CONSOLE_INFORMATION& Globals::getConsoleInformation()
{
    return ciConsoleInformation;
}

bool Globals::IsHeadless() const
{
    return launchArgs.IsHeadless();
}
