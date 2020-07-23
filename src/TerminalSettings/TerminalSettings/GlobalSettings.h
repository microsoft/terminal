#pragma once

namespace winrt::TerminalSettings::implementation
{
    enum class ElementTheme
    {
        TODO_FIX
    };

    enum class TabViewWidthMode
    {
        TODO_FIX
    };

    struct LaunchPosition
    {
        int x;
        int y;
    };

    enum class AppLaunchMode
    {
        TODO_FIX
    };

    class GlobalSettings
    {
        GlobalSettings() = default;
        ~GlobalSettings() = default;

        int InitialRows;
        int InitialCols;
        bool AlwaysShowTabs;
        bool ShowTitleInTitlebar;
        bool ConfirmCloseAllTabs;
        ElementTheme Theme;
        TabViewWidthMode TabWidthMode;
        bool ShowTabsInTitlebar;
        hstring WordDelimiters;
        bool CopyOnSelect;
        bool CopyFormatting;
        bool WarnAboutLargePaste;
        bool WarnAboutMultiLinePaste;
        LaunchPosition InitialPosition;
        AppLaunchMode LaunchMode;
        bool SnapToGridOnResize;
        bool ForceFullRepaintRendering;
        bool SoftwareRendering;
        bool ForceVTInput;
        bool DebugFeaturesEnabled;
        bool StartOnUserLogin;
        bool AlwaysOnTop;
    };

}
