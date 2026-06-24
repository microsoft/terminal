/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MTSMSettings.h

Abstract:
- Contains most of the settings within Terminal Settings Model (global, profile, font, appearance)
- To add a new setting to any one of those classes, simply add it to the respective list below, following the macro format
- Also defines SettingKey enums for generic setting access

Author(s):
- Pankaj Bhojwani - October 2021

--*/
#pragma once

// Macro format (defaultArgs are optional):
// (type, name, jsonKey, defaultArgs)
//
// Each MTSM_*_SETTINGS list is split into two groups so the getter/setter
// generation can dispatch on storage shape while the bookkeeping passes
// (ToJson, HasSetting, ClearSetting, enum, JsonKeyForSetting, LayerJson
// logging) see one unified list:
//
//   MTSM_*_SETTINGS_SCALARS — regular JSON-backed scalar settings.
//     Use with INHERITABLE_SETTING / INHERITABLE_SETTING_WITH_LOGGING.
//
//   MTSM_*_SETTINGS_COLLECTIONS — IVector<T> / IMap<K,V> settings whose
//     callers mutate the returned container in place.
//     Use with INHERITABLE_JSON_BACKED_VECTOR_SETTING /
//     INHERITABLE_JSON_BACKED_MAP_SETTING.
//
//   MTSM_*_SETTINGS — unifies both for the bookkeeping passes.

#define MTSM_GLOBAL_SETTINGS_SCALARS(X)                                                                                                                                                         \
    X(int32_t, InitialRows, "initialRows", 30)                                                                                                                                                  \
    X(int32_t, InitialCols, "initialCols", 80)                                                                                                                                                  \
    X(hstring, WordDelimiters, "wordDelimiters", DEFAULT_WORD_DELIMITERS)                                                                                                                       \
    X(bool, CopyOnSelect, "copyOnSelect", false)                                                                                                                                                \
    X(bool, FocusFollowMouse, "focusFollowMouse", false)                                                                                                                                        \
    X(bool, ScrollToZoom, "experimental.scrollToZoom", true)                                                                                                                                    \
    X(bool, ScrollToChangeOpacity, "experimental.scrollToChangeOpacity", true)                                                                                                                  \
    X(winrt::Microsoft::Terminal::Control::GraphicsAPI, GraphicsAPI, "rendering.graphicsAPI")                                                                                                   \
    X(bool, DisablePartialInvalidation, "rendering.disablePartialInvalidation", false)                                                                                                          \
    X(bool, SoftwareRendering, "rendering.software", false)                                                                                                                                     \
    X(winrt::Microsoft::Terminal::Control::TextMeasurement, TextMeasurement, "compatibility.textMeasurement")                                                                                   \
    X(winrt::Microsoft::Terminal::Control::AmbiguousWidth, AmbiguousWidth, "compatibility.ambiguousWidth", winrt::Microsoft::Terminal::Control::AmbiguousWidth::Narrow)                         \
    X(winrt::Microsoft::Terminal::Control::DefaultInputScope, DefaultInputScope, "defaultInputScope")                                                                                           \
    X(bool, UseBackgroundImageForWindow, "experimental.useBackgroundImageForWindow", false)                                                                                                     \
    X(bool, TrimBlockSelection, "trimBlockSelection", true)                                                                                                                                     \
    X(bool, DetectURLs, "experimental.detectURLs", true)                                                                                                                                        \
    X(bool, AlwaysShowTabs, "alwaysShowTabs", true)                                                                                                                                             \
    X(Model::NewTabPosition, NewTabPosition, "newTabPosition", Model::NewTabPosition::AfterLastTab)                                                                                             \
    X(bool, ShowTitleInTitlebar, "showTerminalTitleInTitlebar", true)                                                                                                                           \
    X(Model::ConfirmOnClose, ConfirmOnClose, "warning.confirmOnClose", Model::ConfirmOnClose::Automatic)                                                                                        \
    X(Model::ThemePair, Theme, "theme")                                                                                                                                                         \
    X(hstring, Language, "language")                                                                                                                                                            \
    X(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, "tabWidthMode", winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal)                                      \
    X(bool, UseAcrylicInTabRow, "useAcrylicInTabRow", false)                                                                                                                                    \
    X(bool, ShowTabsInTitlebar, "showTabsInTitlebar", true)                                                                                                                                     \
    X(bool, InputServiceWarning, "warning.inputService", true)                                                                                                                                  \
    X(winrt::Microsoft::Terminal::Control::CopyFormat, CopyFormatting, "copyFormatting", 0)                                                                                                     \
    X(bool, WarnAboutLargePaste, "warning.largePaste", true)                                                                                                                                    \
    X(winrt::Microsoft::Terminal::Control::WarnAboutMultiLinePaste, WarnAboutMultiLinePaste, "warning.multiLinePaste", winrt::Microsoft::Terminal::Control::WarnAboutMultiLinePaste::Automatic) \
    X(Model::LaunchPosition, InitialPosition, "initialPosition", nullptr, nullptr)                                                                                                              \
    X(bool, CenterOnLaunch, "centerOnLaunch", false)                                                                                                                                            \
    X(Model::FirstWindowPreference, FirstWindowPreference, "firstWindowPreference", FirstWindowPreference::DefaultProfile)                                                                      \
    X(Model::LaunchMode, LaunchMode, "launchMode", LaunchMode::DefaultMode)                                                                                                                     \
    X(bool, SnapToGridOnResize, "snapToGridOnResize", true)                                                                                                                                     \
    X(bool, DebugFeaturesEnabled, "debugFeatures", debugFeaturesDefault)                                                                                                                        \
    X(bool, AlwaysOnTop, "alwaysOnTop", false)                                                                                                                                                  \
    X(bool, AutoHideWindow, "autoHideWindow", false)                                                                                                                                            \
    X(Model::TabSwitcherMode, TabSwitcherMode, "tabSwitcherMode", Model::TabSwitcherMode::InOrder)                                                                                              \
    X(bool, DisableAnimations, "disableAnimations", false)                                                                                                                                      \
    X(hstring, StartupActions, "startupActions", L"")                                                                                                                                           \
    X(Model::WindowingMode, WindowingBehavior, "windowingBehavior", Model::WindowingMode::UseNew)                                                                                               \
    X(bool, MinimizeToNotificationArea, "minimizeToNotificationArea", false)                                                                                                                    \
    X(bool, AlwaysShowNotificationIcon, "alwaysShowNotificationIcon", false)                                                                                                                    \
    X(bool, ShowAdminShield, "showAdminShield", true)                                                                                                                                           \
    X(bool, TrimPaste, "trimPaste", true)                                                                                                                                                       \
    X(bool, EnableColorSelection, "experimental.enableColorSelection", false)                                                                                                                   \
    X(bool, EnableShellCompletionMenu, "experimental.enableShellCompletionMenu", false)                                                                                                         \
    X(bool, EnableUnfocusedAcrylic, "compatibility.enableUnfocusedAcrylic", true)                                                                                                               \
    X(bool, AllowHeadless, "compatibility.allowHeadless", false)                                                                                                                                \
    X(hstring, SearchWebDefaultQueryUrl, "searchWebDefaultQueryUrl", L"https://www.bing.com/search?q=%22%s%22")                                                                                 \
    X(bool, ShowTabsFullscreen, "showTabsFullscreen", false)

#define MTSM_GLOBAL_SETTINGS_COLLECTIONS(X)                                                                                        \
    X(winrt::Windows::Foundation::Collections::IVector<winrt::hstring>, DisabledProfileSources, "disabledProfileSources", nullptr) \
    X(winrt::Windows::Foundation::Collections::IVector<winrt::hstring>, SafeUriSchemes, "safeUriSchemes", nullptr)

#define MTSM_GLOBAL_SETTINGS(X)     \
    MTSM_GLOBAL_SETTINGS_SCALARS(X) \
    MTSM_GLOBAL_SETTINGS_COLLECTIONS(X)

// Also add these settings to:
// * Profile.idl
// * TerminalSettings.h
// * TerminalSettings.cpp: TerminalSettings::_ApplyProfileSettings
// * IControlSettings.idl or ICoreSettings.idl
// * ControlProperties.h
#define MTSM_PROFILE_SETTINGS_SCALARS(X)                                                                                                                       \
    X(int32_t, HistorySize, "historySize", DEFAULT_HISTORY_SIZE)                                                                                               \
    X(bool, SnapOnInput, "snapOnInput", true)                                                                                                                  \
    X(bool, AltGrAliasing, "altGrAliasing", true)                                                                                                              \
    X(hstring, AnswerbackMessage, "answerbackMessage")                                                                                                         \
    X(hstring, Commandline, "commandline", L"%SystemRoot%\\System32\\cmd.exe")                                                                                 \
    X(Microsoft::Terminal::Control::ScrollbarState, ScrollState, "scrollbarState", Microsoft::Terminal::Control::ScrollbarState::Visible)                      \
    X(Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, "antialiasingMode", Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale) \
    X(hstring, StartingDirectory, "startingDirectory")                                                                                                         \
    X(bool, SuppressApplicationTitle, "suppressApplicationTitle", false)                                                                                       \
    X(guid, ConnectionType, "connectionType")                                                                                                                  \
    X(CloseOnExitMode, CloseOnExit, "closeOnExit", CloseOnExitMode::Automatic)                                                                                 \
    X(hstring, TabTitle, "tabTitle")                                                                                                                           \
    X(Model::BellStyle, BellStyle, "bellStyle", BellStyle::Audible)                                                                                            \
    X(bool, RightClickContextMenu, "rightClickContextMenu", false)                                                                                             \
    X(bool, Elevate, "elevate", false)                                                                                                                         \
    X(bool, AutoMarkPrompts, "autoMarkPrompts", true)                                                                                                          \
    X(bool, ShowMarks, "showMarksOnScrollbar", false)                                                                                                          \
    X(bool, RepositionCursorWithMouse, "experimental.repositionCursorWithMouse", false)                                                                        \
    X(bool, ReloadEnvironmentVariables, "compatibility.reloadEnvironmentVariables", true)                                                                      \
    X(bool, RainbowSuggestions, "experimental.rainbowSuggestions", false)                                                                                      \
    X(bool, ForceVTInput, "compatibility.input.forceVT", false)                                                                                                \
    X(bool, AllowKittyKeyboardMode, "compatibility.kittyKeyboardMode", true)                                                                                   \
    X(bool, AllowVtChecksumReport, "compatibility.allowDECRQCRA", false)                                                                                       \
    X(bool, AllowVtClipboardWrite, "compatibility.allowOSC52", true)                                                                                           \
    X(bool, AllowOscNotifications, "compatibility.allowOSC777", false)                                                                                         \
    X(bool, AllowKeypadMode, "compatibility.allowDECNKM", false)                                                                                               \
    X(hstring, DragDropDelimiter, "dragDropDelimiter", L" ")                                                                                                   \
    X(Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle, "pathTranslationStyle", Microsoft::Terminal::Control::PathTranslationStyle::None)

#define MTSM_PROFILE_SETTINGS_COLLECTIONS(X) \
    X(IEnvironmentVariableMap, EnvironmentVariables, "environment", nullptr)

#define MTSM_PROFILE_SETTINGS(X)     \
    MTSM_PROFILE_SETTINGS_SCALARS(X) \
    MTSM_PROFILE_SETTINGS_COLLECTIONS(X)

// Intentionally omitted Profile settings:
// * Name
// * Updates
// * Guid
// * Hidden
// * Source
// * Padding: needs special FromJson parsing
// * TabColor: is an optional setting, so needs to be set with INHERITABLE_NULLABLE_SETTING

#define MTSM_FONT_SETTINGS_SCALARS(X)                                                  \
    X(hstring, FontFace, "face", DEFAULT_FONT_FACE)                                    \
    X(float, FontSize, "size", DEFAULT_FONT_SIZE)                                      \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight, "weight", DEFAULT_FONT_WEIGHT) \
    X(bool, EnableBuiltinGlyphs, "builtinGlyphs", true)                                \
    X(bool, EnableColorGlyphs, "colorGlyphs", true)                                    \
    X(winrt::hstring, CellWidth, "cellWidth")                                          \
    X(winrt::hstring, CellHeight, "cellHeight")

#define MTSM_FONT_SETTINGS_COLLECTIONS(X) \
    X(IFontAxesMap, FontAxes, "axes")     \
    X(IFontFeatureMap, FontFeatures, "features")

#define MTSM_FONT_SETTINGS(X)     \
    MTSM_FONT_SETTINGS_SCALARS(X) \
    MTSM_FONT_SETTINGS_COLLECTIONS(X)

#define MTSM_APPEARANCE_SETTINGS(X)                                                                                                                                \
    X(Core::CursorStyle, CursorShape, "cursorShape", Core::CursorStyle::Bar)                                                                                       \
    X(uint32_t, CursorHeight, "cursorHeight", DEFAULT_CURSOR_HEIGHT)                                                                                               \
    X(float, BackgroundImageOpacity, "backgroundImageOpacity", 1.0f)                                                                                               \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, "backgroundImageStretchMode", winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill) \
    X(bool, RetroTerminalEffect, "experimental.retroTerminalEffect", false)                                                                                        \
    X(ConvergedAlignment, BackgroundImageAlignment, "backgroundImageAlignment", ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center)       \
    X(Model::IntenseStyle, IntenseTextStyle, "intenseTextStyle", Model::IntenseStyle::Bright)                                                                      \
    X(Core::AdjustTextMode, AdjustIndistinguishableColors, "adjustIndistinguishableColors", Core::AdjustTextMode::Automatic)                                       \
    X(bool, UseAcrylic, "useAcrylic", false)

// Intentionally omitted Appearance settings:
// * ForegroundKey, BackgroundKey, SelectionBackgroundKey, CursorColorKey: all optional colors
// * Opacity: needs special parsing

#define MTSM_THEME_SETTINGS(X)                                                                   \
    X(winrt::Microsoft::Terminal::Settings::Model::WindowTheme, Window, "window", nullptr)       \
    X(winrt::Microsoft::Terminal::Settings::Model::SettingsTheme, Settings, "settings", nullptr) \
    X(winrt::Microsoft::Terminal::Settings::Model::TabRowTheme, TabRow, "tabRow", nullptr)       \
    X(winrt::Microsoft::Terminal::Settings::Model::TabTheme, Tab, "tab", nullptr)

#define MTSM_THEME_WINDOW_SETTINGS(X)                                                                                              \
    X(winrt::Windows::UI::Xaml::ElementTheme, RequestedTheme, "applicationTheme", winrt::Windows::UI::Xaml::ElementTheme::Default) \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, Frame, "frame", nullptr)                                            \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, UnfocusedFrame, "unfocusedFrame", nullptr)                          \
    X(bool, RainbowFrame, "experimental.rainbowFrame", false)                                                                      \
    X(bool, UseMica, "useMica", false)                                                                                             \
    X(bool, ShowWorkspacesButton, "showWorkspacesButton", true)

#define MTSM_THEME_SETTINGS_SETTINGS(X) \
    X(winrt::Windows::UI::Xaml::ElementTheme, RequestedTheme, "theme", winrt::Windows::UI::Xaml::ElementTheme::Default)

#define MTSM_THEME_TABROW_SETTINGS(X)                                                             \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, Background, "background", nullptr) \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, UnfocusedBackground, "unfocusedBackground", nullptr)

#define MTSM_THEME_TAB_SETTINGS(X)                                                                                                                     \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, Background, "background", nullptr)                                                      \
    X(winrt::Microsoft::Terminal::Settings::Model::ThemeColor, UnfocusedBackground, "unfocusedBackground", nullptr)                                    \
    X(winrt::Microsoft::Terminal::Settings::Model::IconStyle, IconStyle, "iconStyle", winrt::Microsoft::Terminal::Settings::Model::IconStyle::Default) \
    X(winrt::Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility, ShowCloseButton, "showCloseButton", winrt::Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility::Always)

// SettingKey enums: provide a generic way to reference settings by key.
// Generated from the MTSM macros above. Also includes special-cased settings
// that are not part of the macro lists (prefixed with _ to avoid name collisions).
// SETTINGS_SIZE is a sentinel value that must remain last in each enum.
namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
#define _MTSM_ENUM_VALUE(type, name, jsonKey, ...) name,

    enum class ProfileSettingKey : int
    {
        MTSM_PROFILE_SETTINGS(_MTSM_ENUM_VALUE)
        // Special-cased profile settings (not in MTSM_PROFILE_SETTINGS due to
        // custom JSON parsing, but included here for completeness)
        _Name,
        _Guid,
        _Source,
        _Hidden,
        _Padding,
        _TabColor,
        // Complex/mutable settings with backing fields
        _Icon,
        _BellSound,
        SETTINGS_SIZE
    };

    enum class GlobalSettingKey : int
    {
        MTSM_GLOBAL_SETTINGS(_MTSM_ENUM_VALUE)
        // Special-cased global settings
        _UnparsedDefaultProfile,
        SETTINGS_SIZE
    };

    enum class FontSettingKey : int
    {
        MTSM_FONT_SETTINGS(_MTSM_ENUM_VALUE)
        SETTINGS_SIZE
    };

    enum class AppearanceSettingKey : int
    {
        MTSM_APPEARANCE_SETTINGS(_MTSM_ENUM_VALUE)
        // Special-cased appearance settings
        _Foreground,
        _Background,
        _SelectionBackground,
        _CursorColor,
        _Opacity,
        _DarkColorSchemeName,
        _LightColorSchemeName,
        // Complex/mutable settings with backing fields
        _PixelShaderPath,
        _PixelShaderImagePath,
        _BackgroundImagePath,
        SETTINGS_SIZE
    };

#undef _MTSM_ENUM_VALUE

    // JSON key lookup: returns the JSON key string for a given SettingKey.
    // Generated from the same macros to maintain a single source of truth.
    constexpr std::string_view JsonKeyForSetting(ProfileSettingKey key)
    {
#define _MTSM_KEY_CASE(type, name, jsonKey, ...) \
    case ProfileSettingKey::name:                \
        return jsonKey;
        switch (key)
        {
            MTSM_PROFILE_SETTINGS(_MTSM_KEY_CASE)
        case ProfileSettingKey::_Name:
            return "name";
        case ProfileSettingKey::_Guid:
            return "guid";
        case ProfileSettingKey::_Source:
            return "source";
        case ProfileSettingKey::_Hidden:
            return "hidden";
        case ProfileSettingKey::_Padding:
            return "padding";
        case ProfileSettingKey::_TabColor:
            return "tabColor";
        case ProfileSettingKey::_Icon:
            return "icon";
        case ProfileSettingKey::_BellSound:
            return "bellSound";
        default:
            return {};
        }
#undef _MTSM_KEY_CASE
    }

    constexpr std::string_view JsonKeyForSetting(GlobalSettingKey key)
    {
#define _MTSM_KEY_CASE(type, name, jsonKey, ...) \
    case GlobalSettingKey::name:                 \
        return jsonKey;
        switch (key)
        {
            MTSM_GLOBAL_SETTINGS(_MTSM_KEY_CASE)
        case GlobalSettingKey::_UnparsedDefaultProfile:
            return "defaultProfile";
        default:
            return {};
        }
#undef _MTSM_KEY_CASE
    }

    constexpr std::string_view JsonKeyForSetting(FontSettingKey key)
    {
#define _MTSM_KEY_CASE(type, name, jsonKey, ...) \
    case FontSettingKey::name:                   \
        return jsonKey;
        switch (key)
        {
            MTSM_FONT_SETTINGS(_MTSM_KEY_CASE)
        default:
            return {};
        }
#undef _MTSM_KEY_CASE
    }

    constexpr std::string_view JsonKeyForSetting(AppearanceSettingKey key)
    {
#define _MTSM_KEY_CASE(type, name, jsonKey, ...) \
    case AppearanceSettingKey::name:             \
        return jsonKey;
        switch (key)
        {
            MTSM_APPEARANCE_SETTINGS(_MTSM_KEY_CASE)
        case AppearanceSettingKey::_Foreground:
            return "foreground";
        case AppearanceSettingKey::_Background:
            return "background";
        case AppearanceSettingKey::_SelectionBackground:
            return "selectionBackground";
        case AppearanceSettingKey::_CursorColor:
            return "cursorColor";
        case AppearanceSettingKey::_Opacity:
            return "opacity";
        case AppearanceSettingKey::_DarkColorSchemeName:
            return "colorScheme";
        case AppearanceSettingKey::_LightColorSchemeName:
            return "colorScheme";
        case AppearanceSettingKey::_PixelShaderPath:
            return "experimental.pixelShaderPath";
        case AppearanceSettingKey::_PixelShaderImagePath:
            return "experimental.pixelShaderImagePath";
        case AppearanceSettingKey::_BackgroundImagePath:
            return "backgroundImage";
        default:
            return {};
        }
#undef _MTSM_KEY_CASE
    }
}
