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
#include "../inc/cppwinrt_utils.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

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
        TerminalSettingsCreateResult(Model::TerminalSettings defaultSettings, Model::TerminalSettings unfocusedSettings) :
            _defaultSettings(defaultSettings),
            _unfocusedSettings(unfocusedSettings) {}

        TerminalSettingsCreateResult(Model::TerminalSettings defaultSettings) :
            _defaultSettings(defaultSettings),
            _unfocusedSettings(nullptr) {}

        Model::TerminalSettings DefaultSettings() { return _defaultSettings; };
        Model::TerminalSettings UnfocusedSettings() { return _unfocusedSettings; };

    private:
        Model::TerminalSettings _defaultSettings;
        Model::TerminalSettings _unfocusedSettings;
    };

    struct TerminalSettings : TerminalSettingsT<TerminalSettings>, IInheritable<TerminalSettings>
    {
        TerminalSettings() = default;

        static Model::TerminalSettingsCreateResult CreateWithProfileByID(const Model::CascadiaSettings& appSettings,
                                                                         guid profileGuid,
                                                                         const Control::IKeyBindings& keybindings);

        static Model::TerminalSettingsCreateResult CreateWithNewTerminalArgs(const Model::CascadiaSettings& appSettings,
                                                                             const Model::NewTerminalArgs& newTerminalArgs,
                                                                             const Control::IKeyBindings& keybindings);

        static Model::TerminalSettingsCreateResult CreateWithParent(const Model::TerminalSettingsCreateResult& parent);

        Model::TerminalSettings GetParent();

        void SetParent(const Model::TerminalSettings& parent);

        void ApplyColorScheme(const Model::ColorScheme& scheme);

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        // GetColorTableEntry needs to be implemented manually, to get a
        // particular value from the array.
        Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept;
        void ColorTable(std::array<Microsoft::Terminal::Core::Color, 16> colors);
        std::array<Microsoft::Terminal::Core::Color, 16> ColorTable();

        INHERITABLE_SETTING(Model::TerminalSettings, til::color, DefaultForeground, DEFAULT_FOREGROUND);
        INHERITABLE_SETTING(Model::TerminalSettings, til::color, DefaultBackground, DEFAULT_BACKGROUND);
        INHERITABLE_SETTING(Model::TerminalSettings, til::color, SelectionBackground, DEFAULT_FOREGROUND);
        INHERITABLE_SETTING(Model::TerminalSettings, int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        INHERITABLE_SETTING(Model::TerminalSettings, int32_t, InitialRows, 30);
        INHERITABLE_SETTING(Model::TerminalSettings, int32_t, InitialCols, 80);

        INHERITABLE_SETTING(Model::TerminalSettings, bool, SnapOnInput, true);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, AltGrAliasing, true);
        INHERITABLE_SETTING(Model::TerminalSettings, til::color, CursorColor, DEFAULT_CURSOR_COLOR);
        INHERITABLE_SETTING(Model::TerminalSettings, Microsoft::Terminal::Core::CursorStyle, CursorShape, Core::CursorStyle::Vintage);
        INHERITABLE_SETTING(Model::TerminalSettings, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, CopyOnSelect, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, InputServiceWarning, true);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, FocusFollowMouse, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, TrimBlockSelection, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, DetectURLs, true);

        INHERITABLE_SETTING(Model::TerminalSettings, Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, TabColor, nullptr);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.
        INHERITABLE_SETTING(Model::TerminalSettings, Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr);

        // ------------------------ End of Core Settings -----------------------

        INHERITABLE_SETTING(Model::TerminalSettings, hstring, ProfileName);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, UseAcrylic, false);
        INHERITABLE_SETTING(Model::TerminalSettings, double, TintOpacity, 0.5);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, Padding, DEFAULT_PADDING);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, FontFace, DEFAULT_FONT_FACE);
        INHERITABLE_SETTING(Model::TerminalSettings, int32_t, FontSize, DEFAULT_FONT_SIZE);

        INHERITABLE_SETTING(Model::TerminalSettings, winrt::Windows::UI::Text::FontWeight, FontWeight);

        INHERITABLE_SETTING(Model::TerminalSettings, hstring, BackgroundImage);
        INHERITABLE_SETTING(Model::TerminalSettings, double, BackgroundImageOpacity, 1.0);

        INHERITABLE_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        INHERITABLE_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        INHERITABLE_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        INHERITABLE_SETTING(Model::TerminalSettings, Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr);

        INHERITABLE_SETTING(Model::TerminalSettings, hstring, Commandline);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, StartingDirectory);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, StartingTitle);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, SuppressApplicationTitle);
        INHERITABLE_SETTING(Model::TerminalSettings, hstring, EnvironmentVariables);

        INHERITABLE_SETTING(Model::TerminalSettings, Microsoft::Terminal::Control::ScrollbarState, ScrollState, Microsoft::Terminal::Control::ScrollbarState::Visible);

        INHERITABLE_SETTING(Model::TerminalSettings, Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale);

        INHERITABLE_SETTING(Model::TerminalSettings, bool, RetroTerminalEffect, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, ForceFullRepaintRendering, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, SoftwareRendering, false);
        INHERITABLE_SETTING(Model::TerminalSettings, bool, ForceVTInput, false);

        INHERITABLE_SETTING(Model::TerminalSettings, hstring, PixelShaderPath);

    private:
        std::optional<std::array<Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE>> _ColorTable;
        gsl::span<Microsoft::Terminal::Core::Color> _getColorTableImpl();
        void _ApplyProfileSettings(const Model::Profile& profile);

        void _ApplyGlobalSettings(const Model::GlobalAppSettings& globalSettings) noexcept;
        void _ApplyAppearanceSettings(const Microsoft::Terminal::Settings::Model::IAppearanceConfig& appearance,
                                      const Windows::Foundation::Collections::IMapView<hstring, Microsoft::Terminal::Settings::Model::ColorScheme>& schemes);

        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(TerminalSettingsCreateResult);
    BASIC_FACTORY(TerminalSettings);
}
