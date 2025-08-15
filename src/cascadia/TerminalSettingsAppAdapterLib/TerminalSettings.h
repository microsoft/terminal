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

#define XXXSETTING(projectedType, type, name, ...)                          \
private:                                                                    \
    std::optional<type> _##name{ std::nullopt };                            \
                                                                            \
    std::optional<type> _get##name##Impl() const                            \
    {                                                                       \
        /*return user set value*/                                           \
        if (_##name)                                                        \
        {                                                                   \
            return _##name;                                                 \
        }                                                                   \
                                                                            \
        /*user set value was not set*/                                      \
        /*iterate through parents to find a value*/                         \
        for (const auto& parent : _parents)                                 \
        {                                                                   \
            if (auto val{ parent->_get##name##Impl() })                     \
            {                                                               \
                return val;                                                 \
            }                                                               \
        }                                                                   \
                                                                            \
        /*no value was found*/                                              \
        return std::nullopt;                                                \
    }                                                                       \
                                                                            \
public:                                                                     \
    /* Returns the resolved value for this setting */                       \
    /* fallback: user set value --> inherited value --> system set value */ \
    type name() const                                                       \
    {                                                                       \
        const auto val{ _get##name##Impl() };                               \
        return val ? *val : type{ __VA_ARGS__ };                            \
    }                                                                       \
                                                                            \
    /* Overwrite the user set value */                                      \
    void name(const type& value)                                            \
    {                                                                       \
        _##name = value;                                                    \
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

        XXXSETTING(TerminalSettings, til::color, DefaultForeground, DEFAULT_FOREGROUND);
        XXXSETTING(TerminalSettings, til::color, DefaultBackground, DEFAULT_BACKGROUND);
        XXXSETTING(TerminalSettings, til::color, SelectionBackground, DEFAULT_FOREGROUND);
        XXXSETTING(TerminalSettings, int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        XXXSETTING(TerminalSettings, int32_t, InitialRows, 30);
        XXXSETTING(TerminalSettings, int32_t, InitialCols, 80);

        XXXSETTING(TerminalSettings, bool, SnapOnInput, true);
        XXXSETTING(TerminalSettings, bool, AltGrAliasing, true);
        XXXSETTING(Model::TerminalSettings, hstring, AnswerbackMessage);
        XXXSETTING(TerminalSettings, til::color, CursorColor, DEFAULT_CURSOR_COLOR);
        XXXSETTING(TerminalSettings, Microsoft::Terminal::Core::CursorStyle, CursorShape, Core::CursorStyle::Vintage);
        XXXSETTING(TerminalSettings, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        XXXSETTING(TerminalSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        XXXSETTING(TerminalSettings, bool, CopyOnSelect, false);
        XXXSETTING(TerminalSettings, Microsoft::Terminal::Control::CopyFormat, CopyFormatting, 0);
        XXXSETTING(TerminalSettings, bool, FocusFollowMouse, false);
        XXXSETTING(Model::TerminalSettings, bool, ScrollToZoom, true);
        XXXSETTING(Model::TerminalSettings, bool, ScrollToChangeOpacity, true);
        XXXSETTING(Model::TerminalSettings, bool, AllowVtChecksumReport, false);
        XXXSETTING(TerminalSettings, bool, TrimBlockSelection, true);
        XXXSETTING(TerminalSettings, bool, DetectURLs, true);
        XXXSETTING(Model::TerminalSettings, bool, AllowVtClipboardWrite, true);

        XXXSETTING(TerminalSettings, Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, TabColor, nullptr);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.
        XXXSETTING(TerminalSettings, Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);

        XXXSETTING(TerminalSettings, bool, IntenseIsBold);
        XXXSETTING(TerminalSettings, bool, IntenseIsBright);

        XXXSETTING(TerminalSettings, Microsoft::Terminal::Core::AdjustTextMode, AdjustIndistinguishableColors, Core::AdjustTextMode::Never);
        XXXSETTING(Model::TerminalSettings, bool, RainbowSuggestions, false);

        // ------------------------ End of Core Settings -----------------------

        XXXSETTING(TerminalSettings, hstring, ProfileName);

        XXXSETTING(TerminalSettings, guid, SessionId);
        XXXSETTING(TerminalSettings, bool, EnableUnfocusedAcrylic, false);
        XXXSETTING(TerminalSettings, bool, UseAcrylic, false);
        XXXSETTING(TerminalSettings, float, Opacity, UseAcrylic() ? 0.5f : 1.0f);
        XXXSETTING(TerminalSettings, hstring, Padding, DEFAULT_PADDING);
        XXXSETTING(TerminalSettings, hstring, FontFace, DEFAULT_FONT_FACE);
        XXXSETTING(TerminalSettings, float, FontSize, DEFAULT_FONT_SIZE);

        XXXSETTING(TerminalSettings, winrt::Windows::UI::Text::FontWeight, FontWeight);
        XXXSETTING(TerminalSettings, IFontAxesMap, FontAxes);
        XXXSETTING(TerminalSettings, IFontFeatureMap, FontFeatures);
        XXXSETTING(TerminalSettings, bool, EnableBuiltinGlyphs, true);
        XXXSETTING(TerminalSettings, bool, EnableColorGlyphs, true);
        XXXSETTING(TerminalSettings, hstring, CellWidth);
        XXXSETTING(TerminalSettings, hstring, CellHeight);

        XXXSETTING(TerminalSettings, hstring, BackgroundImage);
        XXXSETTING(TerminalSettings, float, BackgroundImageOpacity, 1.0f);

        XXXSETTING(TerminalSettings, winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        XXXSETTING(TerminalSettings, winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        XXXSETTING(TerminalSettings, winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        XXXSETTING(TerminalSettings, Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr);

        XXXSETTING(TerminalSettings, hstring, Commandline);
        XXXSETTING(TerminalSettings, hstring, StartingDirectory);
        XXXSETTING(TerminalSettings, hstring, StartingTitle);
        XXXSETTING(TerminalSettings, bool, SuppressApplicationTitle);
        XXXSETTING(TerminalSettings, IEnvironmentVariableMap, EnvironmentVariables);

        XXXSETTING(TerminalSettings, Microsoft::Terminal::Control::ScrollbarState, ScrollState, Microsoft::Terminal::Control::ScrollbarState::Visible);

        XXXSETTING(TerminalSettings, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale);

        XXXSETTING(TerminalSettings, bool, RetroTerminalEffect, false);
        XXXSETTING(TerminalSettings, Microsoft::Terminal::Control::GraphicsAPI, GraphicsAPI);
        XXXSETTING(TerminalSettings, bool, DisablePartialInvalidation, false);
        XXXSETTING(TerminalSettings, bool, SoftwareRendering, false);
        XXXSETTING(Model::TerminalSettings, Microsoft::Terminal::Control::TextMeasurement, TextMeasurement);
        XXXSETTING(Model::TerminalSettings, Microsoft::Terminal::Control::DefaultInputScope, DefaultInputScope);
        XXXSETTING(TerminalSettings, bool, UseBackgroundImageForWindow, false);
        XXXSETTING(TerminalSettings, bool, ForceVTInput, false);

        XXXSETTING(TerminalSettings, hstring, PixelShaderPath);
        XXXSETTING(TerminalSettings, hstring, PixelShaderImagePath);

        XXXSETTING(TerminalSettings, bool, Elevate, false);

        XXXSETTING(TerminalSettings, bool, AutoMarkPrompts, false);
        XXXSETTING(TerminalSettings, bool, ShowMarks, false);
        XXXSETTING(TerminalSettings, bool, RightClickContextMenu, false);
        XXXSETTING(TerminalSettings, bool, RepositionCursorWithMouse, false);

        XXXSETTING(TerminalSettings, bool, ReloadEnvironmentVariables, true);

        XXXSETTING(Model::TerminalSettings, Microsoft::Terminal::Control::PathTranslationStyle, PathTranslationStyle, Microsoft::Terminal::Control::PathTranslationStyle::None);

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
