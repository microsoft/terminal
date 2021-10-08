/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MTSMSettings.h

Abstract:
- 

Author(s):
- 

--*/
#pragma once

#define MTSM_GLOBAL_SETTINGS(X)                                                                                                                 \
    X(int32_t, InitialRows, 30)                                                                                                            \
    X(int32_t, InitialCols, 80)                                                                                                            \
    X(hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS)                                                                                    \
    X(bool, CopyOnSelect, false)                                                                                                           \
    X(bool, FocusFollowMouse, false)                                                                                                       \
    X(bool, ForceFullRepaintRendering, false)                                                                                              \
    X(bool, SoftwareRendering, false)                                                                                                      \
    X(bool, ForceVTInput, false)                                                                                                           \
    X(bool, TrimBlockSelection, false)                                                                                                     \
    X(bool, DetectURLs, true)                                                                                                              \
    X(bool, AlwaysShowTabs, true)                                                                                                          \
    X(bool, ShowTitleInTitlebar, true)                                                                                                     \
    X(bool, ConfirmCloseAllTabs, true)                                                                                                     \
    X(hstring, Language)                                                                                                                   \
    X(winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default)                                      \
    X(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal) \
    X(bool, UseAcrylicInTabRow, false)                                                                                                     \
    X(bool, ShowTabsInTitlebar, true)                                                                                                      \
    X(bool, InputServiceWarning, true)                                                                                                     \
    X(winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, 0)                                                                  \
    X(bool, WarnAboutLargePaste, true)                                                                                                     \
    X(bool, WarnAboutMultiLinePaste, true)                                                                                                 \
    X(Model::LaunchPosition, InitialPosition, nullptr, nullptr)                                                                            \
    X(bool, CenterOnLaunch, false)                                                                                                         \
    X(Model::FirstWindowPreference, FirstWindowPreference, FirstWindowPreference::DefaultProfile)                                          \
    X(Model::LaunchMode, LaunchMode, LaunchMode::DefaultMode)                                                                              \
    X(bool, SnapToGridOnResize, true)                                                                                                      \
    X(bool, DebugFeaturesEnabled, debugFeaturesDefault)                                                                                    \
    X(bool, StartOnUserLogin, false)                                                                                                       \
    X(bool, AlwaysOnTop, false)                                                                                                            \
    X(Model::TabSwitcherMode, TabSwitcherMode, Model::TabSwitcherMode::InOrder)                                                            \
    X(bool, DisableAnimations, false)                                                                                                      \
    X(hstring, StartupActions, L"")                                                                                                        \
    X(Model::WindowingMode, WindowingBehavior, Model::WindowingMode::UseNew)                                                               \
    X(bool, MinimizeToNotificationArea, false)                                                                                             \
    X(bool, AlwaysShowNotificationIcon, false)                                                                                             \
    X(winrt::Windows::Foundation::Collections::IVector<winrt::hstring>, DisabledProfileSources, nullptr)                                   \
    X(bool, ShowAdminShield, true)

#define MTSM_PROFILE_SETTINGS(X)                                                                                                                \
    X(int32_t, HistorySize, DEFAULT_HISTORY_SIZE)                                                                                          \
    X(bool, SnapOnInput, true)                                                                                                             \
    X(bool, AltGrAliasing, true)                                                                                                           \
    X(bool, UseAcrylic, false)                                                                                                             \
    X(hstring, Padding, DEFAULT_PADDING)                                                                                                   \
    X(hstring, Commandline, L"%SystemRoot%\\System32\\cmd.exe")                                                                                                    \
    X(Microsoft::Terminal::Control::ScrollbarState, ScrollState, Microsoft::Terminal::Control::ScrollbarState::Visible)                    \
    X(Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale) \
    X(hstring, StartingDirectory)                                                                                                          \
    X(bool, SuppressApplicationTitle, false)                                                                                               \
    X(guid, ConnectionType)                                                                                                                \
    X(hstring, Icon, L"\uE756")                                                                                                            \
    X(CloseOnExitMode, CloseOnExit, CloseOnExitMode::Graceful)                                                                             \
    X(hstring, TabTitle)                                                                                                                   \
    X(Model::BellStyle, BellStyle, BellStyle::Audible)

#define MTSM_FONT_SETTINGS(X)                                                     \
    X(hstring, FontFace, DEFAULT_FONT_FACE)                                  \
    X(int32_t, FontSize, DEFAULT_FONT_SIZE)                                  \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight, DEFAULT_FONT_WEIGHT) \
    X(IFontAxesMap, FontAxes)                                                \
    X(IFontFeatureMap, FontFeatures)

#define MTSM_APPEARANCE_SETTINGS(X)                                                                                                       \
    X(Core::CursorStyle, CursorShape, Core::CursorStyle::Bar)                                                                        \
    X(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT)                                                                                 \
    X(double, BackgroundImageOpacity, 1.0)                                                                                           \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill) \
    X(bool, RetroTerminalEffect, false)                                                                                              \
    X(hstring, PixelShaderPath)                                                                                                      \
    X(ConvergedAlignment, BackgroundImageAlignment, ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center)     \
    X(hstring, ColorSchemeName, L"Campbell")                                                                                         \
    X(hstring, BackgroundImagePath)                                                                                                  \
    X(Model::IntenseStyle, IntenseTextStyle, Model::IntenseStyle::Bright)                                                            \
    X(double, Opacity, 1.0)                                                                                                          \
    X(bool, AdjustIndistinguishableColors, true)
