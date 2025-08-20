/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TerminalSettings.h

Abstract:
- The implementation of the TerminalSettings winrt class. Provides both
        terminal control settings and terminal core settings.
Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include <../TerminalSettingsModel/IInheritable.h>
#include <DefaultSettings.h>
#include <conattrs.hpp>

#define SIMPLE_INHERITABLE_SETTING(type, name, ...)   \
private:                                              \
    std::optional<type> _##name{ std::nullopt };      \
                                                      \
public:                                               \
    /* Returns the resolved value for this setting */ \
    type name() const                                 \
    {                                                 \
        if (_##name.has_value())                      \
        {                                             \
            return *_##name;                          \
        }                                             \
        for (const auto& parent : _parents)           \
        {                                             \
            if (parent->_##name.has_value())          \
            {                                         \
                return *parent->_##name;              \
            }                                         \
        }                                             \
        return type{ __VA_ARGS__ };                   \
    }                                                 \
                                                      \
    /* Overwrite the user set value */                \
    void name(const type& value)                      \
    {                                                 \
        _##name = value;                              \
    }

namespace winrt::Microsoft::Terminal::Settings
{
    using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IEnvironmentVariableMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>;

    struct TerminalSettingsCreateResult;

    struct TerminalSettings :
        winrt::implements<TerminalSettings, winrt::Microsoft::Terminal::Core::ICoreSettings, winrt::Microsoft::Terminal::Control::IControlSettings, winrt::Microsoft::Terminal::Core::ICoreAppearance, winrt::Microsoft::Terminal::Control::IControlAppearance>,
        winrt::Microsoft::Terminal::Settings::Model::implementation::IInheritable<TerminalSettings>
    {
        TerminalSettings() = default;

        static winrt::com_ptr<TerminalSettings> CreateForPreview(const Model::CascadiaSettings& appSettings, const Model::Profile& profile);

        static TerminalSettingsCreateResult CreateWithProfile(const Model::CascadiaSettings& appSettings,
                                                              const Model::Profile& profile,
                                                              const Control::IKeyBindings& keybindings);

        static TerminalSettingsCreateResult CreateWithNewTerminalArgs(const Model::CascadiaSettings& appSettings,
                                                                      const Model::NewTerminalArgs& newTerminalArgs,
                                                                      const Control::IKeyBindings& keybindings);

        void ApplyColorScheme(const Model::ColorScheme& scheme);

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        // GetColorTableEntry needs to be implemented manually, to get a
        // particular value from the array.
        Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept;
        void ColorTable(std::array<Microsoft::Terminal::Core::Color, 16> colors);
        std::array<Microsoft::Terminal::Core::Color, 16> ColorTable();

        SIMPLE_INHERITABLE_SETTING(til::color, DefaultForeground, DEFAULT_FOREGROUND);
        SIMPLE_INHERITABLE_SETTING(til::color, DefaultBackground, DEFAULT_BACKGROUND);
        SIMPLE_INHERITABLE_SETTING(til::color, SelectionBackground, DEFAULT_FOREGROUND);
        SIMPLE_INHERITABLE_SETTING(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        SIMPLE_INHERITABLE_SETTING(int32_t, InitialRows, 30);
        SIMPLE_INHERITABLE_SETTING(int32_t, InitialCols, 80);

        SIMPLE_INHERITABLE_SETTING(bool, SnapOnInput, true);
        SIMPLE_INHERITABLE_SETTING(bool, AltGrAliasing, true);
        SIMPLE_INHERITABLE_SETTING(hstring, AnswerbackMessage);
        SIMPLE_INHERITABLE_SETTING(til::color, CursorColor, DEFAULT_CURSOR_COLOR);
        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Core::CursorStyle, CursorShape, Core::CursorStyle::Vintage);
        SIMPLE_INHERITABLE_SETTING(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        SIMPLE_INHERITABLE_SETTING(hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        SIMPLE_INHERITABLE_SETTING(bool, CopyOnSelect, false);
        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::CopyFormat, CopyFormatting, 0);
        SIMPLE_INHERITABLE_SETTING(bool, FocusFollowMouse, false);
        SIMPLE_INHERITABLE_SETTING(bool, ScrollToZoom, true);
        SIMPLE_INHERITABLE_SETTING(bool, ScrollToChangeOpacity, true);
        SIMPLE_INHERITABLE_SETTING(bool, AllowVtChecksumReport, false);
        SIMPLE_INHERITABLE_SETTING(bool, TrimBlockSelection, true);
        SIMPLE_INHERITABLE_SETTING(bool, DetectURLs, true);
        SIMPLE_INHERITABLE_SETTING(bool, AllowVtClipboardWrite, true);

        SIMPLE_INHERITABLE_SETTING(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, TabColor, nullptr);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.
        SIMPLE_INHERITABLE_SETTING(Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);

        SIMPLE_INHERITABLE_SETTING(bool, IntenseIsBold);
        SIMPLE_INHERITABLE_SETTING(bool, IntenseIsBright);

        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Core::AdjustTextMode, AdjustIndistinguishableColors, Core::AdjustTextMode::Never);
        SIMPLE_INHERITABLE_SETTING(bool, RainbowSuggestions, false);

        // ------------------------ End of Core Settings -----------------------

        SIMPLE_INHERITABLE_SETTING(guid, SessionId);
        SIMPLE_INHERITABLE_SETTING(bool, EnableUnfocusedAcrylic, false);
        SIMPLE_INHERITABLE_SETTING(bool, UseAcrylic, false);
        SIMPLE_INHERITABLE_SETTING(float, Opacity, UseAcrylic() ? 0.5f : 1.0f);
        SIMPLE_INHERITABLE_SETTING(hstring, Padding, DEFAULT_PADDING);
        SIMPLE_INHERITABLE_SETTING(hstring, FontFace, DEFAULT_FONT_FACE);
        SIMPLE_INHERITABLE_SETTING(float, FontSize, DEFAULT_FONT_SIZE);

        SIMPLE_INHERITABLE_SETTING(winrt::Windows::UI::Text::FontWeight, FontWeight);
        SIMPLE_INHERITABLE_SETTING(IFontAxesMap, FontAxes);
        SIMPLE_INHERITABLE_SETTING(IFontFeatureMap, FontFeatures);
        SIMPLE_INHERITABLE_SETTING(bool, EnableBuiltinGlyphs, true);
        SIMPLE_INHERITABLE_SETTING(bool, EnableColorGlyphs, true);
        SIMPLE_INHERITABLE_SETTING(hstring, CellWidth);
        SIMPLE_INHERITABLE_SETTING(hstring, CellHeight);

        SIMPLE_INHERITABLE_SETTING(hstring, BackgroundImage);
        SIMPLE_INHERITABLE_SETTING(float, BackgroundImageOpacity, 1.0f);

        SIMPLE_INHERITABLE_SETTING(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        SIMPLE_INHERITABLE_SETTING(winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        SIMPLE_INHERITABLE_SETTING(winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr);

        SIMPLE_INHERITABLE_SETTING(hstring, Commandline);
        SIMPLE_INHERITABLE_SETTING(hstring, StartingDirectory);
        SIMPLE_INHERITABLE_SETTING(hstring, StartingTitle);
        SIMPLE_INHERITABLE_SETTING(bool, SuppressApplicationTitle);
        SIMPLE_INHERITABLE_SETTING(IEnvironmentVariableMap, EnvironmentVariables);

        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::ScrollbarState, ScrollState, Microsoft::Terminal::Control::ScrollbarState::Visible);

        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale);

        SIMPLE_INHERITABLE_SETTING(bool, RetroTerminalEffect, false);
        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::GraphicsAPI, GraphicsAPI);
        SIMPLE_INHERITABLE_SETTING(bool, DisablePartialInvalidation, false);
        SIMPLE_INHERITABLE_SETTING(bool, SoftwareRendering, false);
        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::TextMeasurement, TextMeasurement);
        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::DefaultInputScope, DefaultInputScope);
        SIMPLE_INHERITABLE_SETTING(bool, UseBackgroundImageForWindow, false);
        SIMPLE_INHERITABLE_SETTING(bool, ForceVTInput, false);

        SIMPLE_INHERITABLE_SETTING(hstring, PixelShaderPath);
        SIMPLE_INHERITABLE_SETTING(hstring, PixelShaderImagePath);

        SIMPLE_INHERITABLE_SETTING(bool, Elevate, false);

        SIMPLE_INHERITABLE_SETTING(bool, AutoMarkPrompts, false);
        SIMPLE_INHERITABLE_SETTING(bool, ShowMarks, false);
        SIMPLE_INHERITABLE_SETTING(bool, RightClickContextMenu, false);
        SIMPLE_INHERITABLE_SETTING(bool, RepositionCursorWithMouse, false);

        SIMPLE_INHERITABLE_SETTING(bool, ReloadEnvironmentVariables, true);

        SIMPLE_INHERITABLE_SETTING(Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle, Microsoft::Terminal::Control::PathTranslationStyle::None);

    private:
        std::optional<std::array<Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE>> _ColorTable;
        std::span<Microsoft::Terminal::Core::Color> _getColorTableImpl();

        static winrt::com_ptr<TerminalSettings> _CreateWithProfileCommon(const Model::CascadiaSettings& appSettings, const Model::Profile& profile);
        void _ApplyProfileSettings(const Model::Profile& profile);

        void _ApplyGlobalSettings(const Model::GlobalAppSettings& globalSettings) noexcept;
        void _ApplyAppearanceSettings(const Microsoft::Terminal::Settings::Model::IAppearanceConfig& appearance,
                                      const Windows::Foundation::Collections::IMapView<hstring, Microsoft::Terminal::Settings::Model::ColorScheme>& schemes,
                                      const winrt::Microsoft::Terminal::Settings::Model::Theme currentTheme);
    };

    struct TerminalSettingsCreateResult
    {
    public:
        TerminalSettingsCreateResult(TerminalSettings* defaultSettings, TerminalSettings* unfocusedSettings)
        {
            _defaultSettings.copy_from(defaultSettings);
            _unfocusedSettings.copy_from(unfocusedSettings);
        }

        TerminalSettingsCreateResult(TerminalSettings* defaultSettings) :
            _unfocusedSettings{ nullptr }
        {
            _defaultSettings.copy_from(defaultSettings);
        }

        winrt::com_ptr<TerminalSettings> DefaultSettings() const { return _defaultSettings; };
        winrt::com_ptr<TerminalSettings> UnfocusedSettings() const { return _unfocusedSettings; };

    private:
        winrt::com_ptr<TerminalSettings> _defaultSettings;
        winrt::com_ptr<TerminalSettings> _unfocusedSettings;
    };

}
