// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "CommandLinePaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandLinePaletteItem : CommandLinePaletteItemT<CommandLinePaletteItem, PaletteItem>
    {
        CommandLinePaletteItem() = default;
        CommandLinePaletteItem(const winrt::hstring& commandLine);

        WINRT_PROPERTY(winrt::hstring, CommandLine);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandLinePaletteItem);
}
