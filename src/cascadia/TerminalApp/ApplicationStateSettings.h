/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.hpp

Abstract:
- This class encapsulates all of the settings that are owned by terminal.

Author(s):
- Dustin Howett - May 2020

--*/

#pragma once

#include "../inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace TerminalApp
{
    class ApplicationStateSettings final
    {
    public:
        ApplicationStateSettings() = default;
        ~ApplicationStateSettings() = default;

        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

        GETSET_PROPERTY(std::optional<bool>, AlwaysCloseAllTabs, true);

    private:
        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ColorSchemeTests;
    };
}
