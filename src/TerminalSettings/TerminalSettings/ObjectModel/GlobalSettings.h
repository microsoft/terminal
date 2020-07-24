#pragma once

namespace winrt::TerminalSettings::implementation
{
    enum class ElementTheme
    {
        system,
        light,
        dark
    };

    enum class TabViewWidthMode
    {
        equal,
        titleLength,
        compact
    };

    struct LaunchPosition
    {
        int x;
        int y;
    };

    enum class AppLaunchMode
    {
        default,
        maximized
    };

    class GlobalSettings
    {
    public:
        GlobalSettings() = default;
        ~GlobalSettings() = default;

        hstring DefaultProfile = L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";

        int InitialRows = 120;
        int InitialCols = 30;
        bool AlwaysShowTabs = true;
        bool ShowTitleInTitlebar = true;
        bool ConfirmCloseAllTabs = true;
        ElementTheme Theme = ElementTheme::system;
        TabViewWidthMode TabWidthMode = TabViewWidthMode::equal;
        bool ShowTabsInTitlebar = true;
        hstring WordDelimiters = L" /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502";
        bool CopyOnSelect = false;
        bool CopyFormatting = true;
        bool WarnAboutLargePaste = true;
        bool WarnAboutMultiLinePaste = true;
        LaunchPosition InitialPosition{0,0};
        AppLaunchMode LaunchMode = AppLaunchMode::default;
        bool SnapToGridOnResize = true;
        bool ForceFullRepaintRendering = false;
        bool SoftwareRendering = false;
        bool ForceVTInput = true;
        bool DebugFeaturesEnabled = false;
        bool StartOnUserLogin = false;
        bool AlwaysOnTop = false;
    };

}
