// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "CommandLinePaletteItem.g.h"
#include "inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandLinePaletteItem : CommandLinePaletteItemT<CommandLinePaletteItem, PaletteItem>
    {
        CommandLinePaletteItem() = default;
        CommandLinePaletteItem(winrt::hstring const& commandLine);

        GETSET_PROPERTY(winrt::hstring, CommandLine);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandLinePaletteItem);
}
