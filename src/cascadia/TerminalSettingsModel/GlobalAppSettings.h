/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- GlobalAppSettings.h

Abstract:
- This class encapsulates all of the settings that are global to the app, and
    not a part of any particular profile.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "GlobalAppSettings.g.h"
#include "IInheritable.h"

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
    struct GlobalAppSettings : GlobalAppSettingsT<GlobalAppSettings>, IInheritable<GlobalAppSettings>
    {
    public:
        GlobalAppSettings();
        void _FinalizeInheritance() override;
        com_ptr<GlobalAppSettings> Copy() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);
        void RemoveColorScheme(hstring schemeName);

        Model::KeyMapping KeyMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        std::vector<SettingsLoadWarnings> KeybindingsWarnings() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::Command> Commands() noexcept;

        // These are implemented manually to handle the string/GUID exchange
        // by higher layers in the app.
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;
        bool HasUnparsedDefaultProfile() const;
        winrt::hstring UnparsedDefaultProfile() const;
        void UnparsedDefaultProfile(const hstring& value);
        void ClearUnparsedDefaultProfile();

        GETSET_SETTING(Model::GlobalAppSettings, int32_t, InitialRows, DEFAULT_ROWS);
        GETSET_SETTING(Model::GlobalAppSettings, int32_t, InitialCols, DEFAULT_COLS);
        GETSET_SETTING(Model::GlobalAppSettings, bool, AlwaysShowTabs, true);
        GETSET_SETTING(Model::GlobalAppSettings, bool, ShowTitleInTitlebar, true);
        GETSET_SETTING(Model::GlobalAppSettings, bool, ConfirmCloseAllTabs, true);
        GETSET_SETTING(Model::GlobalAppSettings, winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
        GETSET_SETTING(Model::GlobalAppSettings, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        GETSET_SETTING(Model::GlobalAppSettings, bool, ShowTabsInTitlebar, true);
        GETSET_SETTING(Model::GlobalAppSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        GETSET_SETTING(Model::GlobalAppSettings, bool, CopyOnSelect, false);
        GETSET_SETTING(Model::GlobalAppSettings, bool, InputServiceWarning, true);
        GETSET_SETTING(Model::GlobalAppSettings, winrt::Microsoft::Terminal::TerminalControl::CopyFormat, CopyFormatting, 0);
        GETSET_SETTING(Model::GlobalAppSettings, bool, WarnAboutLargePaste, true);
        GETSET_SETTING(Model::GlobalAppSettings, bool, WarnAboutMultiLinePaste, true);
        GETSET_SETTING(Model::GlobalAppSettings, Model::LaunchPosition, InitialPosition, nullptr, nullptr);
        GETSET_SETTING(Model::GlobalAppSettings, bool, CenterOnLaunch, false);
        GETSET_SETTING(Model::GlobalAppSettings, Model::LaunchMode, LaunchMode, LaunchMode::DefaultMode);
        GETSET_SETTING(Model::GlobalAppSettings, bool, SnapToGridOnResize, true);
        GETSET_SETTING(Model::GlobalAppSettings, bool, ForceFullRepaintRendering, false);
        GETSET_SETTING(Model::GlobalAppSettings, bool, SoftwareRendering, false);
        GETSET_SETTING(Model::GlobalAppSettings, bool, ForceVTInput, false);
        GETSET_SETTING(Model::GlobalAppSettings, bool, DebugFeaturesEnabled, _getDefaultDebugFeaturesValue());
        GETSET_SETTING(Model::GlobalAppSettings, bool, StartOnUserLogin, false);
        GETSET_SETTING(Model::GlobalAppSettings, bool, AlwaysOnTop, false);
        GETSET_SETTING(Model::GlobalAppSettings, Model::TabSwitcherMode, TabSwitcherMode, Model::TabSwitcherMode::InOrder);
        GETSET_SETTING(Model::GlobalAppSettings, bool, DisableAnimations, false);
        GETSET_SETTING(Model::GlobalAppSettings, hstring, StartupActions, L"");
        GETSET_SETTING(Model::GlobalAppSettings, bool, FocusFollowMouse, false);
        GETSET_SETTING(Model::GlobalAppSettings, Model::WindowingMode, WindowingBehavior, Model::WindowingMode::UseNew);

    private:
        guid _defaultProfile;
        std::optional<hstring> _UnparsedDefaultProfile{ std::nullopt };
        bool _validDefaultProfile;

        com_ptr<KeyMapping> _keymap;
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;

        Windows::Foundation::Collections::IMap<hstring, Model::ColorScheme> _colorSchemes;
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _commands;

        std::optional<hstring> _getUnparsedDefaultProfileImpl() const;
        static bool _getDefaultDebugFeaturesValue();

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}
