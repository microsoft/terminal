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

#include "KeyMapping.h"
#include "Command.h"
#include "ColorScheme.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class ColorSchemeTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct GlobalAppSettings : GlobalAppSettingsT<GlobalAppSettings>
    {
    public:
        GlobalAppSettings();

        Windows::Foundation::Collections::IObservableMap<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);

        Model::KeyMapping KeyMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        std::vector<SettingsLoadWarnings> KeybindingsWarnings() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::Command> Commands() noexcept;

        // These are implemented manually to handle the string/GUID exchange
        // by higher layers in the app.
        void DefaultProfile(const winrt::guid& defaultProfile) noexcept;
        winrt::guid DefaultProfile() const;
        hstring UnparsedDefaultProfile() const;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(int32_t, InitialRows, _PropertyChangedHandlers, DEFAULT_ROWS);
        OBSERVABLE_GETSET_PROPERTY(int32_t, InitialCols, _PropertyChangedHandlers, DEFAULT_COLS);
        OBSERVABLE_GETSET_PROPERTY(bool, AlwaysShowTabs, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ShowTitleInTitlebar, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ConfirmCloseAllTabs, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, Theme, _PropertyChangedHandlers, winrt::Windows::UI::Xaml::ElementTheme::Default);
        OBSERVABLE_GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, _PropertyChangedHandlers, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        OBSERVABLE_GETSET_PROPERTY(bool, ShowTabsInTitlebar, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(hstring, WordDelimiters, _PropertyChangedHandlers, DEFAULT_WORD_DELIMITERS);
        OBSERVABLE_GETSET_PROPERTY(bool, CopyOnSelect, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(winrt::Microsoft::Terminal::TerminalControl::CopyFormat, CopyFormatting, _PropertyChangedHandlers, 0);
        OBSERVABLE_GETSET_PROPERTY(bool, WarnAboutLargePaste, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, WarnAboutMultiLinePaste, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(Model::LaunchPosition, InitialPosition, _PropertyChangedHandlers, nullptr, nullptr);
        OBSERVABLE_GETSET_PROPERTY(Model::LaunchMode, LaunchMode, _PropertyChangedHandlers, LaunchMode::DefaultMode);
        OBSERVABLE_GETSET_PROPERTY(bool, SnapToGridOnResize, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceFullRepaintRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, SoftwareRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceVTInput, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, DebugFeaturesEnabled, _PropertyChangedHandlers); // default value set in constructor
        OBSERVABLE_GETSET_PROPERTY(bool, StartOnUserLogin, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, AlwaysOnTop, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, UseTabSwitcher, _PropertyChangedHandlers, true);

    private:
        hstring _unparsedDefaultProfile;
        winrt::guid _defaultProfile;

        com_ptr<KeyMapping> _keymap;
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;

        Windows::Foundation::Collections::IObservableMap<hstring, Model::ColorScheme> _colorSchemes;
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _commands;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}
