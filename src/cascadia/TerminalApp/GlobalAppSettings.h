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

#include "GlobalAppSettings.g.h"

#include "AppKeyBindings.h"
#include "Command.h"
#include "ColorScheme.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace winrt::TerminalApp::implementation
{
    struct GlobalAppSettings : GlobalAppSettingsT<GlobalAppSettings>
    {
    public:
        GlobalAppSettings();
        ~GlobalAppSettings();

        Windows::Foundation::Collections::IMapView<hstring, TerminalApp::ColorScheme> GetColorSchemes() noexcept;
        void AddColorScheme(const TerminalApp::ColorScheme& scheme);

        TerminalApp::AppKeyBindings GetKeybindings() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        void ApplyToSettings(const TerminalApp::TerminalSettings& settings) const noexcept;

        std::vector<::TerminalApp::SettingsLoadWarnings> GetKeybindingsWarnings() const;

        Windows::Foundation::Collections::IMapView<hstring, TerminalApp::Command> GetCommands() noexcept;

        // These are implemented manually to handle the string/GUID exchange
        // by higher layers in the app.
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;
        hstring UnparsedDefaultProfile() const;

        GETSET_PROPERTY(int32_t, InitialRows); // default value set in constructor
        GETSET_PROPERTY(int32_t, InitialCols); // default value set in constructor
        GETSET_PROPERTY(bool, AlwaysShowTabs, true);
        GETSET_PROPERTY(bool, ShowTitleInTitlebar, true);
        GETSET_PROPERTY(bool, ConfirmCloseAllTabs, true);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
        GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        GETSET_PROPERTY(bool, ShowTabsInTitlebar, true);
        GETSET_PROPERTY(hstring, WordDelimiters); // default value set in constructor
        GETSET_PROPERTY(bool, CopyOnSelect, false);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::TerminalControl::CopyFormat, CopyFormatting, 0);
        GETSET_PROPERTY(bool, WarnAboutLargePaste, true);
        GETSET_PROPERTY(bool, WarnAboutMultiLinePaste, true);
        GETSET_PROPERTY(winrt::TerminalApp::LaunchPosition, InitialPosition, nullptr, nullptr);
        GETSET_PROPERTY(winrt::TerminalApp::LaunchMode, LaunchMode, winrt::TerminalApp::LaunchMode::DefaultMode);
        GETSET_PROPERTY(bool, SnapToGridOnResize, true);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
        GETSET_PROPERTY(bool, SoftwareRendering, false);
        GETSET_PROPERTY(bool, ForceVTInput, false);
        GETSET_PROPERTY(bool, DebugFeaturesEnabled); // default value set in constructor
        GETSET_PROPERTY(bool, StartOnUserLogin, false);
        GETSET_PROPERTY(bool, AlwaysOnTop, false);
        GETSET_PROPERTY(bool, UseTabSwitcher, true);

    private:
        hstring _unparsedDefaultProfile;
        guid _defaultProfile;

        com_ptr<AppKeyBindings> _keybindings;
        std::vector<::TerminalApp::SettingsLoadWarnings> _keybindingsWarnings;

        Windows::Foundation::Collections::IMap<hstring, TerminalApp::ColorScheme> _colorSchemes;
        Windows::Foundation::Collections::IMap<hstring, TerminalApp::Command> _commands;

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ColorSchemeTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(GlobalAppSettings);
}
