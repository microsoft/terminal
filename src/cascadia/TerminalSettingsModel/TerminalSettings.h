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
#include "IInheritable.h"
#include "../inc/cppwinrt_utils.h"
#include <DefaultSettings.h>
#include <conattrs.hpp>

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct TerminalSettings : TerminalSettingsT<TerminalSettings>, IInheritable<TerminalSettings>
    {
        TerminalSettings() = default;
        TerminalSettings(const Model::CascadiaSettings& appSettings,
                         guid profileGuid,
                         const TerminalControl::IKeyBindings& keybindings);

        static Model::TerminalSettings BuildSettings(const Model::CascadiaSettings& appSettings,
                                                     const Model::NewTerminalArgs& newTerminalArgs,
                                                     const TerminalControl::IKeyBindings& keybindings);

        Model::TerminalSettings MakeChild() { return *CreateChild(); }
        void SetParent(Model::TerminalSettings parent)
        {
            ClearParents();
            com_ptr<TerminalSettings> parentImpl;
            parentImpl.attach(get_self<TerminalSettings>(parent));
            InsertParent(0, parentImpl);
        }

        void ApplyColorScheme(const Model::ColorScheme& scheme);

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.

        // GetColorTableEntry needs to be implemented manually, to get a
        // particular value from the array.
        uint32_t GetColorTableEntry(int32_t index) noexcept;
        void ColorTable(std::array<uint32_t, 16> colors);
        std::array<uint32_t, 16> ColorTable();

        GETSET_SETTING(Model::TerminalSettings, uint32_t, DefaultForeground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_SETTING(Model::TerminalSettings, uint32_t, DefaultBackground, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_SETTING(Model::TerminalSettings, uint32_t, SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_SETTING(Model::TerminalSettings, int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        GETSET_SETTING(Model::TerminalSettings, int32_t, InitialRows, 30);
        GETSET_SETTING(Model::TerminalSettings, int32_t, InitialCols, 80);

        GETSET_SETTING(Model::TerminalSettings, bool, SnapOnInput, true);
        GETSET_SETTING(Model::TerminalSettings, bool, AltGrAliasing, true);
        GETSET_SETTING(Model::TerminalSettings, uint32_t, CursorColor, DEFAULT_CURSOR_COLOR);
        GETSET_SETTING(Model::TerminalSettings, Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, TerminalControl::CursorStyle::Vintage);
        GETSET_SETTING(Model::TerminalSettings, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);
        GETSET_SETTING(Model::TerminalSettings, hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        GETSET_SETTING(Model::TerminalSettings, bool, CopyOnSelect, false);
        GETSET_SETTING(Model::TerminalSettings, bool, InputServiceWarning, true);
        GETSET_SETTING(Model::TerminalSettings, bool, FocusFollowMouse, false);

        GETSET_SETTING(Model::TerminalSettings, Windows::Foundation::IReference<uint32_t>, TabColor, nullptr);

        // When set, StartingTabColor allows to create a terminal with a "sticky" tab color.
        // This color is prioritized above the TabColor (that is usually initialized based on profile settings).
        // Due to this prioritization, the tab color will be preserved upon settings reload
        // (even if the profile's tab color gets altered or removed).
        // This property is expected to be passed only once upon terminal creation.
        // TODO: to ensure that this property is not populated during settings reload,
        // we should consider moving this property to a separate interface,
        // passed to the terminal only upon creation.
        GETSET_SETTING(Model::TerminalSettings, Windows::Foundation::IReference<uint32_t>, StartingTabColor, nullptr);

        // ------------------------ End of Core Settings -----------------------

        GETSET_SETTING(Model::TerminalSettings, hstring, ProfileName);
        GETSET_SETTING(Model::TerminalSettings, bool, UseAcrylic, false);
//    public: /* Returns true if the user explicitly set the value, false otherwise*/
//        bool HasUseAcrylic() const
//        {
//            return _UseAcrylic.has_value();
//        }
//
//        Model::TerminalSettings UseAcrylicOverrideSource()
//        { /*user set value was not set*/ /*iterate through parents to find one with a value*/
//            for (auto& parent : _parents)
//            {
//                if (auto source{ parent->_getUseAcrylicOverrideSourceImpl() })
//                {
//                    return source;
//                }
//            }
//
//            /*no value was found*/
//            return nullptr;
//        }
//
//        /* Returns the resolved value for this setting */ /* fallback: user set value --> inherited value --> system set value */
//        bool UseAcrylic() const
//        {
//            const auto val{ _getUseAcrylicImpl() };
//            return val ? *val : bool{ false };
//        }
//
//        /* Overwrite the user set value */
//        void UseAcrylic(const bool& value)
//        {
//            _UseAcrylic = value;
//        }
//
//        /* Clear the user set value */
//        void ClearUseAcrylic()
//        {
//            _UseAcrylic = std::nullopt;
//        }
//
//    private:
//        std::optional<bool> _UseAcrylic{ std::nullopt };
//        std::optional<bool> _getUseAcrylicImpl() const
//        { /*return user set value*/
//            if (_UseAcrylic)
//            {
//                return _UseAcrylic;
//            }
//
//            /*user set value was not set*/ /*iterate through parents to find a value*/
//            for (auto parent : _parents)
//            {
//                if (auto val{ parent->_getUseAcrylicImpl() })
//                {
//                    return val;
//                }
//            }
//
//            /*no value was found*/
//            return std::nullopt;
//        }
//        Model::TerminalSettings _getUseAcrylicOverrideSourceImpl() const
//        { /*we have a value*/
//            if (_UseAcrylic)
//            {
//                return *this;
//            }
//
//            /*user set value was not set*/ /*iterate through parents to find one with a value*/
//            for (auto& parent : _parents)
//            {
//                if (auto source{ parent->_getUseAcrylicOverrideSourceImpl() })
//                {
//                    return source;
//                }
//            }
//
//            /*no value was found*/
//            return nullptr;
//        }

        GETSET_SETTING(Model::TerminalSettings, double, TintOpacity, 0.5);
        GETSET_SETTING(Model::TerminalSettings, hstring, Padding, DEFAULT_PADDING);
        GETSET_SETTING(Model::TerminalSettings, hstring, FontFace, DEFAULT_FONT_FACE);
        GETSET_SETTING(Model::TerminalSettings, int32_t, FontSize, DEFAULT_FONT_SIZE);

        GETSET_SETTING(Model::TerminalSettings, winrt::Windows::UI::Text::FontWeight, FontWeight);

        GETSET_SETTING(Model::TerminalSettings, hstring, BackgroundImage);
        GETSET_SETTING(Model::TerminalSettings, double, BackgroundImageOpacity, 1.0);

        GETSET_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill);
        GETSET_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center);
        GETSET_SETTING(Model::TerminalSettings, winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center);

        GETSET_SETTING(Model::TerminalSettings, Microsoft::Terminal::TerminalControl::IKeyBindings, KeyBindings, nullptr);

        GETSET_SETTING(Model::TerminalSettings, hstring, Commandline);
        GETSET_SETTING(Model::TerminalSettings, hstring, StartingDirectory);
        GETSET_SETTING(Model::TerminalSettings, hstring, StartingTitle);
        GETSET_SETTING(Model::TerminalSettings, bool, SuppressApplicationTitle);
        GETSET_SETTING(Model::TerminalSettings, hstring, EnvironmentVariables);

        GETSET_SETTING(Model::TerminalSettings, Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState, Microsoft::Terminal::TerminalControl::ScrollbarState::Visible);

        GETSET_SETTING(Model::TerminalSettings, Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::TerminalControl::TextAntialiasingMode::Grayscale);

        GETSET_SETTING(Model::TerminalSettings, bool, RetroTerminalEffect, false);
        GETSET_SETTING(Model::TerminalSettings, bool, ForceFullRepaintRendering, false);
        GETSET_SETTING(Model::TerminalSettings, bool, SoftwareRendering, false);
        GETSET_SETTING(Model::TerminalSettings, bool, ForceVTInput, false);

        GETSET_PROPERTY(hstring, PixelShaderPath);

    private:
        std::optional<std::array<uint32_t, COLOR_TABLE_SIZE>> _ColorTable;
        gsl::span<uint32_t> _getColorTableImpl();
        void _ApplyProfileSettings(const Model::Profile& profile, const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes);
        void _ApplyGlobalSettings(const Model::GlobalAppSettings& globalSettings) noexcept;

        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(TerminalSettings);
}
