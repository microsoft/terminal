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
        GlobalAppSettings();
        void _FinalizeInheritance() override;
        com_ptr<GlobalAppSettings> Copy() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);
        void RemoveColorScheme(hstring schemeName);

        Model::ActionMap ActionMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        std::vector<SettingsLoadWarnings> KeybindingsWarnings() const;

        // These are implemented manually to handle the string/GUID exchange
        // by higher layers in the app.
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;
        bool HasUnparsedDefaultProfile() const;
        winrt::hstring UnparsedDefaultProfile() const;
        void UnparsedDefaultProfile(const hstring& value);
        void ClearUnparsedDefaultProfile();

        INHERITABLE_SETTING(Model::GlobalAppSettings, int32_t, InitialRows, DEFAULT_ROWS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, int32_t, InitialCols, DEFAULT_COLS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, AlwaysShowTabs, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ShowTitleInTitlebar, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ConfirmCloseAllTabs, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ShowTabsInTitlebar, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, CopyOnSelect, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, InputServiceWarning, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, 0);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, WarnAboutLargePaste, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, WarnAboutMultiLinePaste, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::LaunchPosition, InitialPosition, nullptr, nullptr);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, CenterOnLaunch, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::LaunchMode, LaunchMode, LaunchMode::DefaultMode);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, SnapToGridOnResize, true);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ForceFullRepaintRendering, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, SoftwareRendering, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, ForceVTInput, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DebugFeaturesEnabled, _getDefaultDebugFeaturesValue());
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, StartOnUserLogin, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, AlwaysOnTop, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::TabSwitcherMode, TabSwitcherMode, Model::TabSwitcherMode::InOrder);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DisableAnimations, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, hstring, StartupActions, L"");
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, FocusFollowMouse, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, Model::WindowingMode, WindowingBehavior, Model::WindowingMode::UseNew);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, TrimBlockSelection, false);
        INHERITABLE_SETTING(Model::GlobalAppSettings, bool, DetectURLs, true);

    private:
        guid _defaultProfile;
        std::optional<hstring> _UnparsedDefaultProfile{ std::nullopt };
        bool _validDefaultProfile;

        com_ptr<implementation::ActionMap> _actionMap;
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;

        Windows::Foundation::Collections::IMap<hstring, Model::ColorScheme> _colorSchemes;

        std::optional<hstring> _getUnparsedDefaultProfileImpl() const;
        static bool _getDefaultDebugFeaturesValue();

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}
