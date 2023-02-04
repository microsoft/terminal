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

#include "TerminalSettings.g.h"
#include "TerminalSettingsCreateResult.g.h"
#include "IInheritable.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

using IFontAxesMap = WFC::IMap<winrt::hstring, float>;
using IFontFeatureMap = WFC::IMap<winrt::hstring, uint32_t>;

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class TerminalSettingsTests;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct TerminalSettingsCreateResult :
        public TerminalSettingsCreateResultT<TerminalSettingsCreateResult>
    {
    public:
        TerminalSettingsCreateResult(MTSM::TerminalSettings defaultSettings, MTSM::TerminalSettings unfocusedSettings) :
            _defaultSettings(defaultSettings),
            _unfocusedSettings(unfocusedSettings) {}

        TerminalSettingsCreateResult(MTSM::TerminalSettings defaultSettings) :
            _defaultSettings(defaultSettings),
            _unfocusedSettings(nullptr) {}

        MTSM::TerminalSettings DefaultSettings() { return _defaultSettings; };
        MTSM::TerminalSettings UnfocusedSettings() { return _unfocusedSettings; };

    private:
        MTSM::TerminalSettings _defaultSettings;
        MTSM::TerminalSettings _unfocusedSettings;
    };

    struct TerminalSettings : TerminalSettingsT<TerminalSettings>, IInheritable<TerminalSettings>
    {
        TerminalSettings() = default;

        static MTSM::TerminalSettings CreateForPreview(const MTSM::CascadiaSettings& appSettings, const MTSM::Profile& profile);

        static MTSM::TerminalSettingsCreateResult CreateWithProfile(const MTSM::CascadiaSettings& appSettings,
                                                                    const MTSM::Profile& profile,
                                                                    const MTControl::IKeyBindings& keybindings);

        static MTSM::TerminalSettingsCreateResult CreateWithNewTerminalArgs(const MTSM::CascadiaSettings& appSettings,
                                                                            const MTSM::NewTerminalArgs& newTerminalArgs,
                                                                            const MTControl::IKeyBindings& keybindings);

        void ApplyColorScheme(const MTSM::ColorScheme& scheme);

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        // GetColorTableEntry needs to be implemented manually, to get a
        // particular value from the array.
        Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept;
        void ColorTable(std::array<Microsoft::Terminal::Core::Color, 16> colors);
        std::array<Microsoft::Terminal::Core::Color, 16> ColorTable();

        INHERITABLE_SETTING(MTSM::TerminalSettings, til::color, DefaultForeground, DEFAULT_FOREGROUND);
        INHERITABLE_SETTING(MTSM::TerminalSettings, til::color, DefaultBackground, DEFAULT_BACKGROUND);
        INHERITABLE_SETTING(MTSM::TerminalSettings, til::color, SelectionBackground, DEFAULT_FOREGROUND);
        INHERITABLE_SETTING(MTSM::TerminalSettings, int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        INHERITABLE_SETTING(MTSM::TerminalSettings, int32_t, InitialRows, 30);
        INHERITABLE_SETTING(MTSM::TerminalSettings, int32_t, InitialCols, 80);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, SnapOnInput, true);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, AltGrAliasing, true);
        INHERITABLE_SETTING(MTSM::TerminalSettings, til::color, CursorColor, DEFAULT_CURSOR_COLOR);
        INHERITABLE_SETTING(MTSM::TerminalSettings, MTCore::CursorStyle, CursorShape, MTCore::CursorStyle::Vintage);
        INHERITABLE_SETTING(MTSM::TerminalSettings, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, CopyOnSelect, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, FocusFollowMouse, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, TrimBlockSelection, true);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, DetectURLs, true);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, VtPassthrough, false);

        INHERITABLE_SETTING(MTSM::TerminalSettings, WF::IReference<Microsoft::Terminal::Core::Color>, TabColor, nullptr);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.
        INHERITABLE_SETTING(MTSM::TerminalSettings, WF::IReference<Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, IntenseIsBold);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, IntenseIsBright);

        INHERITABLE_SETTING(MTSM::TerminalSettings, MTCore::AdjustTextMode, AdjustIndistinguishableColors, MTCore::AdjustTextMode::Never);

        // ------------------------ End of Core Settings -----------------------

        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, ProfileName);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, ProfileSource);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, UseAcrylic, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, double, Opacity, UseAcrylic() ? 0.5 : 1.0);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, Padding, DEFAULT_PADDING);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, FontFace, DEFAULT_FONT_FACE);
        INHERITABLE_SETTING(MTSM::TerminalSettings, float, FontSize, DEFAULT_FONT_SIZE);

        INHERITABLE_SETTING(MTSM::TerminalSettings, WUT::FontWeight, FontWeight);
        INHERITABLE_SETTING(MTSM::TerminalSettings, IFontAxesMap, FontAxes);
        INHERITABLE_SETTING(MTSM::TerminalSettings, IFontFeatureMap, FontFeatures);

        INHERITABLE_SETTING(MTSM::TerminalSettings, MTSM::ColorScheme, AppliedColorScheme);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, BackgroundImage);
        INHERITABLE_SETTING(MTSM::TerminalSettings, double, BackgroundImageOpacity, 1.0);

        INHERITABLE_SETTING(MTSM::TerminalSettings, WUXMedia::Stretch, BackgroundImageStretchMode, WUXMedia::Stretch::UniformToFill);
        INHERITABLE_SETTING(MTSM::TerminalSettings, WUX::HorizontalAlignment, BackgroundImageHorizontalAlignment, WUX::HorizontalAlignment::Center);
        INHERITABLE_SETTING(MTSM::TerminalSettings, WUX::VerticalAlignment, BackgroundImageVerticalAlignment, WUX::VerticalAlignment::Center);

        INHERITABLE_SETTING(MTSM::TerminalSettings, MTControl::IKeyBindings, KeyBindings, nullptr);

        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, Commandline);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, StartingDirectory);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, StartingTitle);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, SuppressApplicationTitle);
        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, EnvironmentVariables);

        INHERITABLE_SETTING(MTSM::TerminalSettings, MTControl::ScrollbarState, ScrollState, MTControl::ScrollbarState::Visible);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, UseAtlasEngine, false);

        INHERITABLE_SETTING(MTSM::TerminalSettings, MTControl::TextAntialiasingMode, AntialiasingMode, MTControl::TextAntialiasingMode::Grayscale);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, RetroTerminalEffect, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, ForceFullRepaintRendering, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, SoftwareRendering, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, UseBackgroundImageForWindow, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, ForceVTInput, false);

        INHERITABLE_SETTING(MTSM::TerminalSettings, hstring, PixelShaderPath);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, Elevate, false);

        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, AutoMarkPrompts, false);
        INHERITABLE_SETTING(MTSM::TerminalSettings, bool, ShowMarks, false);

    private:
        std::optional<std::array<Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE>> _ColorTable;
        std::span<Microsoft::Terminal::Core::Color> _getColorTableImpl();

        static winrt::com_ptr<implementation::TerminalSettings> _CreateWithProfileCommon(const MTSM::CascadiaSettings& appSettings, const MTSM::Profile& profile);
        void _ApplyProfileSettings(const MTSM::Profile& profile);

        void _ApplyGlobalSettings(const MTSM::GlobalAppSettings& globalSettings) noexcept;
        void _ApplyAppearanceSettings(const MTSM::IAppearanceConfig& appearance,
                                      const WFC::IMapView<hstring, MTSM::ColorScheme>& schemes,
                                      const MTSM::Theme currentTheme);

        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(TerminalSettingsCreateResult);
    BASIC_FACTORY(TerminalSettings);
}
