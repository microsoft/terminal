// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// A helper function for determining the GUID of the current Virtual Desktop.

#pragma once

namespace VirtualDesktopUtils
{
    bool GetCurrentVirtualDesktopId(GUID* desktopId);
}
