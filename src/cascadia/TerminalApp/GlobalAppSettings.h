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

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace TerminalApp
{
    class GlobalAppSettings;

    struct LaunchPosition
    {
        std::optional<int> x;
        std::optional<int> y;
    };
};

class TerminalApp::GlobalAppSettings final
{
public:
    GlobalAppSettings();
    ~GlobalAppSettings();

    std::unordered_map<std::wstring, ColorScheme>& GetColorSchemes() noexcept;
    const std::unordered_map<std::wstring, ColorScheme>& GetColorSchemes() const noexcept;
    void AddColorScheme(ColorScheme scheme);

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    static GlobalAppSettings FromJson(const Json::Value& json);
    void LayerJson(const Json::Value& json);

    void ApplyToSettings(winrt::Microsoft::Terminal::Settings::TerminalSettings& settings) const noexcept;

    std::vector<TerminalApp::SettingsLoadWarnings> GetKeybindingsWarnings() const;

    // These are implemented manually to handle the string/GUID exchange
    // by higher layers in the app.
    void DefaultProfile(const GUID defaultProfile) noexcept;
    GUID DefaultProfile() const;
    std::wstring UnparsedDefaultProfile() const;

    GETSET_PROPERTY(int32_t, InitialRows); // default value set in constructor
    GETSET_PROPERTY(int32_t, InitialCols); // default value set in constructor
    GETSET_PROPERTY(bool, AlwaysShowTabs, true);
    GETSET_PROPERTY(bool, ShowTitleInTitlebar, true);
    GETSET_PROPERTY(bool, ConfirmCloseAllTabs, true);
    GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
    GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
    GETSET_PROPERTY(int, RowsToScroll); // default value set in constructor
    GETSET_PROPERTY(bool, ShowTabsInTitlebar, true);
    GETSET_PROPERTY(std::wstring, WordDelimiters); // default value set in constructor
    GETSET_PROPERTY(bool, CopyOnSelect, false);
    GETSET_PROPERTY(bool, CopyFormatting, false);
    GETSET_PROPERTY(LaunchPosition, InitialPosition);
    GETSET_PROPERTY(winrt::TerminalApp::LaunchMode, LaunchMode, winrt::TerminalApp::LaunchMode::DefaultMode);
    GETSET_PROPERTY(bool, SnapToGridOnResize, true);
    GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
    GETSET_PROPERTY(bool, SoftwareRendering, false);
    GETSET_PROPERTY(bool, ForceVTInput, false);
    GETSET_PROPERTY(bool, DebugFeaturesEnabled); // default value set in constructor

    GETSET_PROPERTY(bool, StartOnUserLogin, false);

private:
    std::optional<std::wstring> _unparsedDefaultProfile;
    GUID _defaultProfile;

    winrt::com_ptr<winrt::TerminalApp::implementation::AppKeyBindings> _keybindings;
    std::vector<::TerminalApp::SettingsLoadWarnings> _keybindingsWarnings;

    std::unordered_map<std::wstring, ColorScheme> _colorSchemes;

    static winrt::Windows::UI::Xaml::ElementTheme _ParseTheme(const std::wstring& themeString) noexcept;

    static winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode _ParseTabWidthMode(const std::wstring& tabWidthModeString) noexcept;

    static void _ParseInitialPosition(const std::string& initialPosition,
                                      LaunchPosition& ret) noexcept;

    static winrt::TerminalApp::LaunchMode _ParseLaunchMode(const std::wstring& launchModeString) noexcept;

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
};
