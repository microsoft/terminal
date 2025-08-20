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
#include <../inc/ControlProperties.h>
#include <DefaultSettings.h>
#include <conattrs.hpp>

#define SIMPLE_OVERRIDABLE_SETTING(type, name, ...)   \
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
    }

namespace winrt::Microsoft::Terminal::Settings
{
    using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using IEnvironmentVariableMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>;

    struct TerminalSettingsCreateResult;

    struct TerminalSettings :
        winrt::implements<TerminalSettings, winrt::Microsoft::Terminal::Core::ICoreSettings, winrt::Microsoft::Terminal::Control::IControlSettings, winrt::Microsoft::Terminal::Core::ICoreAppearance, winrt::Microsoft::Terminal::Core::ICoreScheme, winrt::Microsoft::Terminal::Control::IControlAppearance>,
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

        void GetColorTable(winrt::com_array<Microsoft::Terminal::Core::Color>& table) noexcept;
        void SetColorTable(std::array<Microsoft::Terminal::Core::Color, 16> colors);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.

        CORE_APPEARANCE_SETTINGS(SIMPLE_OVERRIDABLE_SETTING);
        CONTROL_APPEARANCE_SETTINGS(SIMPLE_OVERRIDABLE_SETTING);
        CORE_SETTINGS(SIMPLE_OVERRIDABLE_SETTING);
        CONTROL_SETTINGS(SIMPLE_OVERRIDABLE_SETTING);
#if 0
        SIMPLE_OVERRIDABLE_SETTING(float, Opacity, UseAcrylic() ? 0.5f : 1.0f);
        SIMPLE_OVERRIDABLE_SETTING(hstring, FontFace, DEFAULT_FONT_FACE);
#endif

        // Settings which do not pertain to the control (why are they here?)
        SIMPLE_OVERRIDABLE_SETTING(bool, Elevate, false);
        SIMPLE_OVERRIDABLE_SETTING(IEnvironmentVariableMap, EnvironmentVariables);
        SIMPLE_OVERRIDABLE_SETTING(bool, ReloadEnvironmentVariables, true);

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
