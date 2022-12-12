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

#ifdef UNIT_TESTING
// Method Description:
// - This is a test helper method. It can be used to trick us into responding
//   true to `IsHeadless`, which will cause the console host to act in conpty
//   mode.
// Arguments:
// - vtRenderEngine: a VT renderer that our VtIo should use as the vt engine during these tests
// Return Value:
// - <none>
void Globals::EnableConptyModeForTests(std::unique_ptr<Microsoft::Console::Render::VtEngine> vtRenderEngine)
{
    launchArgs.EnableConptyModeForTests();
    getConsoleInformation().GetVtIo()->EnableConptyModeForTests(std::move(vtRenderEngine));
}
#endif
