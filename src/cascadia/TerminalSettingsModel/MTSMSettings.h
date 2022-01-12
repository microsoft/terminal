/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MTSMSettings.h

Abstract:
- Contains most of the settings within Terminal Settings Model (global, profile, font, appearance)
- To add a new setting to any one of those classes, simply add it to the respective list below, following the macro format

Author(s):
- Pankaj Bhojwani - October 2021

--*/
#pragma once

// Macro format (defaultArgs are optional):
// (type, name, jsonKey, defaultArgs)

#define MTSM_GLOBAL_SETTINGS(X)                                                                                                                            \
    X(int32_t, InitialRows, "initialRows", 30)                                                                                                             \
    X(int32_t, InitialCols, "initialCols", 80)                                                                                                             \
    X(hstring, WordDelimiters, "wordDelimiters", DEFAULT_WORD_DELIMITERS)                                                                                  \
    X(bool, CopyOnSelect, "copyOnSelect", false)                                                                                                           \
    X(bool, FocusFollowMouse, "focusFollowMouse", false)                                                                                                   \
    X(bool, ForceFullRepaintRendering, "experimental.rendering.forceFullRepaint", false)                                                                   \
    X(bool, SoftwareRendering, "experimental.rendering.software", false)                                                                                   \
    X(bool, ForceVTInput, "experimental.input.forceVT", false)                                                                                             \
    X(bool, TrimBlockSelection, "trimBlockSelection", false)                                                                                               \
    X(bool, DetectURLs, "experimental.detectURLs", true)                                                                                                   \
    X(bool, AlwaysShowTabs, "alwaysShowTabs", true)                                                                                                        \
    X(bool, ShowTitleInTitlebar, "showTerminalTitleInTitlebar", true)                                                                                      \
    X(bool, ConfirmCloseAllTabs, "confirmCloseAllTabs", true)                                                                                              \
    X(hstring, Language, "language")                                                                                                                       \
    X(winrt::Windows::UI::Xaml::ElementTheme, Theme, "theme", winrt::Windows::UI::Xaml::ElementTheme::Default)                                             \
    X(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, "tabWidthMode", winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal) \
    X(bool, UseAcrylicInTabRow, "useAcrylicInTabRow", false)                                                                                               \
    X(bool, ShowTabsInTitlebar, "showTabsInTitlebar", true)                                                                                                \
    X(bool, InputServiceWarning, "inputServiceWarning", true)                                                                                              \
    X(winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, "copyFormatting", 0)                                                                \
    X(bool, WarnAboutLargePaste, "largePasteWarning", true)                                                                                                \
    X(bool, WarnAboutMultiLinePaste, "multiLinePasteWarning", true)                                                                                        \
    X(Model::LaunchPosition, InitialPosition, "initialPosition", nullptr, nullptr)                                                                         \
    X(bool, CenterOnLaunch, "centerOnLaunch", false)                                                                                                       \
    X(Model::FirstWindowPreference, FirstWindowPreference, "firstWindowPreference", FirstWindowPreference::DefaultProfile)                                 \
    X(Model::LaunchMode, LaunchMode, "launchMode", LaunchMode::DefaultMode)                                                                                \
    X(bool, SnapToGridOnResize, "snapToGridOnResize", true)                                                                                                \
    X(bool, DebugFeaturesEnabled, "debugFeatures", debugFeaturesDefault)                                                                                   \
    X(bool, StartOnUserLogin, "startOnUserLogin", false)                                                                                                   \
    X(bool, AlwaysOnTop, "alwaysOnTop", false)                                                                                                             \
    X(Model::TabSwitcherMode, TabSwitcherMode, "tabSwitcherMode", Model::TabSwitcherMode::InOrder)                                                         \
    X(bool, DisableAnimations, "disableAnimations", false)                                                                                                 \
    X(hstring, StartupActions, "startupActions", L"")                                                                                                      \
    X(Model::WindowingMode, WindowingBehavior, "windowingBehavior", Model::WindowingMode::UseNew)                                                          \
    X(bool, MinimizeToNotificationArea, "minimizeToNotificationArea", false)                                                                               \
    X(bool, AlwaysShowNotificationIcon, "alwaysShowNotificationIcon", false)                                                                               \
    X(winrt::Windows::Foundation::Collections::IVector<winrt::hstring>, DisabledProfileSources, "disabledProfileSources", nullptr)                         \
    X(bool, ShowAdminShield, "showAdminShield", true)                                                                                                      \
    X(bool, TrimPaste, "trimPaste", true)

#define MTSM_PROFILE_SETTINGS(X)                                                                                                                               \
    X(int32_t, HistorySize, "historySize", DEFAULT_HISTORY_SIZE)                                                                                               \
    X(bool, SnapOnInput, "snapOnInput", true)                                                                                                                  \
    X(bool, AltGrAliasing, "altGrAliasing", true)                                                                                                              \
    X(bool, UseAcrylic, "useAcrylic", false)                                                                                                                   \
    X(hstring, Commandline, "commandline", L"%SystemRoot%\\System32\\cmd.exe")                                                                                 \
    X(Microsoft::Terminal::Control::ScrollbarState, ScrollState, "scrollbarState", Microsoft::Terminal::Control::ScrollbarState::Visible)                      \
    X(Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, "antialiasingMode", Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale) \
    X(hstring, StartingDirectory, "startingDirectory")                                                                                                         \
    X(bool, SuppressApplicationTitle, "suppressApplicationTitle", false)                                                                                       \
    X(guid, ConnectionType, "connectionType")                                                                                                                  \
    X(hstring, Icon, "icon", L"\uE756")                                                                                                                        \
    X(CloseOnExitMode, CloseOnExit, "closeOnExit", CloseOnExitMode::Graceful)                                                                                  \
    X(hstring, TabTitle, "tabTitle")                                                                                                                           \
    X(Model::BellStyle, BellStyle, "bellStyle", BellStyle::Audible)                                                                                            \
    X(bool, UseAtlasEngine, "experimental.useAtlasEngine", false)                                                                                              \
    X(Windows::Foundation::Collections::IVector<winrt::hstring>, BellSound, "bellSound", nullptr)                                                              \
    X(bool, Elevate, "elevate", false)

#define MTSM_FONT_SETTINGS(X)                                                          \
    X(hstring, FontFace, "face", DEFAULT_FONT_FACE)                                    \
    X(int32_t, FontSize, "size", DEFAULT_FONT_SIZE)                                    \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight, "weight", DEFAULT_FONT_WEIGHT) \
    X(IFontAxesMap, FontAxes, "axes")                                                  \
    X(IFontFeatureMap, FontFeatures, "features")

#define MTSM_APPEARANCE_SETTINGS(X)                                                                                                                                \
    X(Core::CursorStyle, CursorShape, "cursorShape", Core::CursorStyle::Bar)                                                                                       \
    X(uint32_t, CursorHeight, "cursorHeight", DEFAULT_CURSOR_HEIGHT)                                                                                               \
    X(double, BackgroundImageOpacity, "backgroundImageOpacity", 1.0)                                                                                               \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, "backgroundImageStretchMode", winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill) \
    X(bool, RetroTerminalEffect, "experimental.retroTerminalEffect", false)                                                                                        \
    X(hstring, PixelShaderPath, "experimental.pixelShaderPath")                                                                                                    \
    X(ConvergedAlignment, BackgroundImageAlignment, "backgroundImageAlignment", ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center)       \
    X(hstring, ColorSchemeName, "colorScheme", L"Campbell")                                                                                                        \
    X(hstring, BackgroundImagePath, "backgroundImage")                                                                                                             \
    X(Model::IntenseStyle, IntenseTextStyle, "intenseTextStyle", Model::IntenseStyle::Bright)                                                                      \
    X(bool, AdjustIndistinguishableColors, "adjustIndistinguishableColors", true)
