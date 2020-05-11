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
        std::optional<int> x, y;
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

    void DefaultProfile(const GUID defaultProfile) noexcept;
    GUID DefaultProfile() const noexcept;

    winrt::TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

    bool DebugFeaturesEnabled() const noexcept;

    static GlobalAppSettings FromJson(const Json::Value& json);
    void LayerJson(const Json::Value& json);

    void ApplyToSettings(winrt::Microsoft::Terminal::Settings::TerminalSettings& settings) const noexcept;

    std::vector<TerminalApp::SettingsLoadWarnings> GetKeybindingsWarnings() const;

    GETSET_PROPERTY(bool, AlwaysShowTabs, true);
    GETSET_PROPERTY(bool, ShowTitleInTitlebar, true);
    GETSET_PROPERTY(bool, ConfirmCloseAllTabs, true);
    GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
    GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
    GETSET_PROPERTY(bool, ShowTabsInTitlebar, true);
    GETSET_PROPERTY(std::wstring, WordDelimiters); // default value set in constructor
    GETSET_PROPERTY(bool, CopyOnSelect, false);
    GETSET_PROPERTY(bool, CopyFormatting, false);
    GETSET_PROPERTY(LaunchPosition, InitialPosition);
    GETSET_PROPERTY(winrt::TerminalApp::LaunchMode, LaunchMode, winrt::TerminalApp::LaunchMode::DefaultMode);
    GETSET_PROPERTY(bool, SnapToGridOnResize, true);
    GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
    GETSET_PROPERTY(bool, SoftwareRendering, false);

private:
    GUID _defaultProfile;
    winrt::com_ptr<winrt::TerminalApp::implementation::AppKeyBindings> _keybindings;
    std::vector<::TerminalApp::SettingsLoadWarnings> _keybindingsWarnings;

    std::unordered_map<std::wstring, ColorScheme> _colorSchemes;

    int32_t _initialRows;
    int32_t _initialCols;

    int32_t _rowsToScroll;

    bool _debugFeatures;

    static winrt::Windows::UI::Xaml::ElementTheme _ParseTheme(const std::wstring& themeString) noexcept;
    static std::wstring_view _SerializeTheme(const winrt::Windows::UI::Xaml::ElementTheme theme) noexcept;

    static std::wstring_view _SerializeTabWidthMode(const winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode tabWidthMode) noexcept;
    static winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode _ParseTabWidthMode(const std::wstring& tabWidthModeString) noexcept;

    static void _ParseInitialPosition(const std::wstring& initialPosition,
                                      LaunchPosition& ret) noexcept;

    static std::string _SerializeInitialPosition(const LaunchPosition& position) noexcept;,

    static std::wstring_view _SerializeLaunchMode(const winrt::TerminalApp::LaunchMode launchMode) noexcept;
    static winrt::TerminalApp::LaunchMode _ParseLaunchMode(const std::wstring& launchModeString) noexcept;

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
};
