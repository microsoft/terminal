/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.hpp

Abstract:
- This class encapsulates all of the settings that are global to the app, and
    not a part of any particular profile.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once
#include "AppKeyBindings.h"
#include "ColorScheme.h"

namespace TerminalApp
{
    class GlobalAppSettings;
};

class TerminalApp::GlobalAppSettings final
{

public:
    GlobalAppSettings();
    ~GlobalAppSettings();

    const std::vector<ColorScheme>& GetColorSchemes() const noexcept;
    std::vector<ColorScheme>& GetColorSchemes() noexcept;
    void SetDefaultProfile(const GUID defaultProfile) noexcept;
    GUID GetDefaultProfile() const noexcept;

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    bool GetAlwaysShowTabs() const noexcept;
    void SetAlwaysShowTabs(const bool showTabs) noexcept;

    bool GetShowTitleInTitlebar() const noexcept;
    void SetShowTitleInTitlebar(const bool showTitleInTitlebar) noexcept;

    bool GetShowTabsInTitlebar() const noexcept;
    void SetShowTabsInTitlebar(const bool showTabsInTitlebar) noexcept;

    winrt::Windows::UI::Xaml::ElementTheme GetRequestedTheme() const noexcept;

    winrt::Windows::Data::Json::JsonObject ToJson() const;
    static GlobalAppSettings FromJson(winrt::Windows::Data::Json::JsonObject json);

    void ApplyToSettings(winrt::Microsoft::Terminal::Settings::TerminalSettings& settings) const noexcept;

private:
    GUID _defaultProfile;
    winrt::TerminalApp::AppKeyBindings _keybindings;

    std::vector<ColorScheme> _colorSchemes;

    int32_t _initialRows;
    int32_t _initialCols;

    bool _showStatusline;
    bool _alwaysShowTabs;
    bool _showTitleInTitlebar;

    bool _showTabsInTitlebar;
    winrt::Windows::UI::Xaml::ElementTheme _requestedTheme;

    static winrt::Windows::UI::Xaml::ElementTheme _ParseTheme(const std::wstring& themeString) noexcept;
    static std::wstring _SerializeTheme(const winrt::Windows::UI::Xaml::ElementTheme theme) noexcept;

};
