// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ResourceRegistry.h"

namespace Microsoft::Terminal::Dsc
{
    // CommandBuilder:
    //
    //   TerminalDsc.exe <get|set|test|delete|export|schema|manifest>
    //                   [--resource <type>] [--input <json>] [--what-if] [--save]
    //
    // While exactly one resource is registered, --resource is implicit; with
    // two or more it is required (except on `manifest`, which then emits the
    // whole manifest list).
    class CommandBuilder
    {
    public:
        explicit CommandBuilder(ResourceRegistry registry) noexcept :
            _registry{ std::move(registry) }
        {
        }

        // Runs the command line and returns the process exit code. Protocol
        // output goes to `output`; diagnostics go to stderr via Logger.
        int Run(const std::vector<std::wstring_view>& args, std::ostream& output);

    private:
        ResourceRegistry _registry;
    };
}
