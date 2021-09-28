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

#include "ActionMap.h"
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
        void _FinalizeInheritance() override;
        com_ptr<GlobalAppSettings> Copy() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);
        void RemoveColorScheme(hstring schemeName);

        Model::ActionMap ActionMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        const std::vector<SettingsLoadWarnings>& KeybindingsWarnings() const;

        // This DefaultProfile() setter is called by CascadiaSettings,
        // when it parses UnparsedDefaultProfile in _finalizeSettings().
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;

        // TODO GH#9207: Remove this once we have a GlobalAppSettingsViewModel in TerminalSettingsEditor
        void SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue)
        {
            DisableAnimations(!invertedDisableAnimationsValue);
        }

        INHERITABLE_SETTING(Model::GlobalAppSettings, int32_t, InitialRows, DEFAULT_ROWS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, int32_t, InitialCols, DEFAULT_COLS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, AlwaysShowTabs, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ShowTitleInTitlebar, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ConfirmCloseAllTabs, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, Language);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, UseAcrylicInTabRow, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ShowTabsInTitlebar, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, CopyOnSelect, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, InputServiceWarning, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, 0);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, WarnAboutLargePaste, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, WarnAboutMultiLinePaste, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::LaunchPosition, InitialPosition, nullptr, nullptr);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, CenterOnLaunch, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::FirstWindowPreference, FirstWindowPreference, FirstWindowPreference::DefaultProfile);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::LaunchMode, LaunchMode, LaunchMode::DefaultMode);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, SnapToGridOnResize, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ForceFullRepaintRendering, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, SoftwareRendering, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ForceVTInput, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DebugFeaturesEnabled, debugFeaturesDefault);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, StartOnUserLogin, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, AlwaysOnTop, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::TabSwitcherMode, TabSwitcherMode, Model::TabSwitcherMode::InOrder);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DisableAnimations, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, StartupActions, L"");
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, FocusFollowMouse, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::WindowingMode, WindowingBehavior, Model::WindowingMode::UseNew);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, TrimBlockSelection, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DetectURLs, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, MinimizeToNotificationArea, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, AlwaysShowNotificationIcon, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Windows::Foundation::Collections::IVector<winrt::hstring>, DisabledProfileSources, nullptr);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, UnparsedDefaultProfile, L"");
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ShowAdminShield, true);

    private:
#ifdef NDEBUG
        static constexpr bool debugFeaturesDefault{ false };
#else
        static constexpr bool debugFeaturesDefault{ true };
#endif

        winrt::guid _defaultProfile;
        winrt::com_ptr<implementation::ActionMap> _actionMap{ winrt::make_self<implementation::ActionMap>() };
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::ColorScheme> _colorSchemes{ winrt::single_threaded_map<winrt::hstring, Model::ColorScheme>() };
    };
}
