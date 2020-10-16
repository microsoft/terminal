#include "pch.h"
#include "AppSettings.h"

using namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation;

// uncomment and edit these to test data binding

//AppSettings::AppSettings()
//{
//// Global Settings
//Globals = GlobalSettings();
//Globals.DefaultProfile = L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";
//Globals.InitialCols = 120;
//Globals.InitialRows = 30;
//Globals.LaunchMode = AppLaunchMode::default;
//Globals.AlwaysOnTop = false;
//Globals.CopyOnSelect = false;
//Globals.CopyFormatting = true;
//Globals.WordDelimiters = L" /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502";
//Globals.AlwaysShowTabs = true;
//Globals.ShowTabsInTitlebar = true;
//Globals.ShowTitleInTitlebar = true;
//Globals.TabWidthMode = TabViewWidthMode::equal;
//Globals.ConfirmCloseAllTabs = true;
//Globals.StartOnUserLogin = false;
//Globals.Theme = ElementTheme::system;
//Globals.SnapToGridOnResize = true;

//// Profiles
//{
//    // Profile 1
//    auto x = Profile();
//    x.Guid = L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}";
//    x.Name = L"Windows PowerShell";
//    x.Commandline = L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
//    x.Icon = L"ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png";
//    x.ColorScheme = L"Campbell";
//    x.AntialiasingMode = TextAntialiasingMode::grayscale;
//    x.CloseOnExit = CloseOnExitMode::graceful;
//    x.CursorShape = CursorStyle::bar;
//    x.FontFace = L"Cascadia Mono";
//    x.FontSize = 12;
//    x.Hidden = false;
//    x.HistorySize = 9001;
//    x.Padding = L"8, 8, 8, 8";
//    x.SnapOnInput = true;
//    x.AltGrAliasing = true;
//    x.StartingDirectory = L"%USERPROFILE%";
//    x.UseAcrylic = false;

//    Profiles.push_back(x);

//    // Profile 2
//    auto y = Profile();
//    y.Guid = L"{0caa0dad-35be-5f56-a8ff-afceeeaa6101}";
//    y.Name = L"Command Prompt";
//    y.Commandline = L"%SystemRoot%\\System32\\cmd.exe";
//    y.Icon = L"ms-appx:///ProfileIcons/{0caa0dad-35be-5f56-a8ff-afceeeaa6101}.png";
//    y.ColorScheme = L"Campbell";
//    y.AntialiasingMode = TextAntialiasingMode::grayscale;
//    y.CloseOnExit = CloseOnExitMode::graceful;
//    y.CursorShape = CursorStyle::bar;
//    y.FontFace = L"Cascadia Mono";
//    y.FontSize = 12;
//    y.Hidden = false;
//    y.HistorySize = 9001;
//    y.Padding = L"8, 8, 8, 8";
//    y.SnapOnInput = true;
//    y.AltGrAliasing = true;
//    y.StartingDirectory = L"%USERPROFILE%";
//    y.UseAcrylic = false;

//    Profiles.push_back(y);
//}

//// Color Schemes
//{
//    // too much work rn to handle adding color schemes
//    // need to convert string to unsigned int
//}

//// Bindings
//{
//    // too much work rn to handle adding key bindings
//    // we don't have a quick-n-easy way to record keys
//    // also, no representation for keybinding args yet
//}
//}

//AppSettings AppSettings::Clone()
//{
//    // TODO: This is fake. But again, we get this out of the _real_ impl of this
//    auto clone = AppSettings();
//    return clone;
//}
