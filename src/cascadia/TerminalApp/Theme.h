/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Theme.h

Abstract:
- TODO

Author(s):
- Mike Griese - July 2020

--*/
#pragma once
#include <winrt/Microsoft.Terminal.Settings.h>
#include "../../inc/conattrs.hpp"
#include "../inc/cppwinrt_utils.h"
#include "SettingsTypes.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace TerminalApp
{
    class Theme;
};

class TerminalApp::Theme final
{
public:
    Theme();
    ~Theme();

    void ApplyTheme(winrt::Microsoft::Terminal::Settings::TerminalSettings terminalSettings) const;

    static Theme FromJson(const Json::Value& json);
    bool ShouldBeLayered(const Json::Value& json) const;
    void LayerJson(const Json::Value& json);

    static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

    GETSET_PROPERTY(winrt::hstring, Name);
    GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, ApplicationTheme, winrt::Windows::UI::Xaml::ElementTheme::Default);
    GETSET_PROPERTY(::TerminalApp::ThemeColor, TabRowBackground);
    GETSET_PROPERTY(::TerminalApp::ThemeColor, TabBackground);

private:
    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
};
