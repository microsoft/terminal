// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "pch.h"

// This enumerates all the possible actions
// that our tray icon context menu could do.
enum class TrayMenuItemAction
{
    FocusTerminal, // Focus the MRU terminal.
    SummonWindow
};
