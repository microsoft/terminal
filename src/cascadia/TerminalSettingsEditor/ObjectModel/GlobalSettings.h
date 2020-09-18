#pragma once
#include "Model.GlobalSettings.g.h"
#include <winrt/Windows.UI.h>
#include "../Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    struct GlobalSettings : GlobalSettingsT<GlobalSettings>
    {
    public:
        GlobalSettings() = default;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(hstring, DefaultProfile, _PropertyChangedHandlers, L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}");
        OBSERVABLE_GETSET_PROPERTY(int, InitialRows, _PropertyChangedHandlers, 120);
        OBSERVABLE_GETSET_PROPERTY(int, InitialCols, _PropertyChangedHandlers, 30);
        OBSERVABLE_GETSET_PROPERTY(bool, AlwaysShowTabs, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ShowTitlebar, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ShowTitleInTitlebar, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ConfirmCloseAllTabs, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Xaml::ElementTheme, Theme, _PropertyChangedHandlers, Windows::UI::Xaml::ElementTheme::Default);
        OBSERVABLE_GETSET_PROPERTY(TabViewWidthMode, TabWidthMode, _PropertyChangedHandlers, TabViewWidthMode::equal);
        OBSERVABLE_GETSET_PROPERTY(bool, ShowTabsInTitlebar, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(hstring, WordDelimiters, _PropertyChangedHandlers, L" /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502");
        OBSERVABLE_GETSET_PROPERTY(bool, CopyOnSelect, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, CopyFormatting, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, WarnAboutLargePaste, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, WarnAboutMultiLinePaste, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(hstring, LaunchPosition, _PropertyChangedHandlers, L"(0,0)");
        OBSERVABLE_GETSET_PROPERTY(AppLaunchMode, LaunchMode, _PropertyChangedHandlers, AppLaunchMode::Default);
        OBSERVABLE_GETSET_PROPERTY(bool, SnapToGridOnResize, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceFullRepaintRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, SoftwareRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceVTInput, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, DebugFeaturesEnabled, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, StartOnUserLogin, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, AlwaysOnTop, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, DisableDynamicProfiles, _PropertyChangedHandlers, false);
    };
}
