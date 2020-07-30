/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsTypes.h

Abstract:
- Types used in the settings model (non-exported)
--*/

#pragma once

namespace TerminalApp
{
    enum class CloseOnExitMode
    {
        Never = 0,
        Graceful,
        Always
    };

    struct LaunchPosition
    {
        std::optional<int> x;
        std::optional<int> y;
    };

    enum class ColorType
    {
        Value = 0,
        Accent,
        TerminalBackground,
        TerminalForeground,
        ResourceKey
    };

    struct ThemeColor
    {
        ColorType type{ ColorType::Value };
        std::optional<til::color> value{ 0xff00ff };
    };
};
