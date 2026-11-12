// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Terminal::Dsc
{
    // What the "set" operation returns on stdout, mirrored into the manifest's
    // "set.return" field.
    enum class SetReturn
    {
        None,
        State,
        StateAndDiff,
    };

    // One documented process exit code, emitted into the manifest's "exitCodes" map.
    struct ExitCodeInfo
    {
        int code;
        std::string_view description;
    };

    // Static description of a Microsoft DSC resource, used for command routing
    // and manifest generation.
    struct ResourceMetadata
    {
        // Fully qualified resource type, e.g. "Microsoft.WindowsTerminal/Settings".
        std::string_view type;
        // Semantic version of the resource (independent of the app version).
        std::string_view version;
        std::string_view description;
        std::vector<std::string_view> tags;
        SetReturn setReturn = SetReturn::State;
        // Whether the manifest marks the get input as mandatory. Resources
        // without key properties leave this false so a bare
        // `dsc resource get` keeps working.
        bool getRequiresInput = false;
        std::vector<ExitCodeInfo> exitCodes;
    };
}
