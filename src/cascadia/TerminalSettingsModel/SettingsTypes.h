/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsTypes.h

Abstract:
- Types used in the settings model (non-exported)
--*/

#pragma once

namespace winrt::Microsoft::Terminal::Settings::Model
{
    enum class ExpandCommandType : uint32_t
    {
        None = 0,
        Profiles,
        ColorSchemes
    };
};
