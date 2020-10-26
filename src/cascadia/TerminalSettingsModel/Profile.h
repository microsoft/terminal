/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Profile.hpp

Abstract:
- A profile acts as a single set of terminal settings. Many tabs or panes could
     exist side-by-side with different profiles simultaneously.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "Profile.g.h"
#include "IInheritable.h"

#include "../inc/cppwinrt_utils.h"
#include "JsonUtils.h"
#include <DefaultSettings.h>

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class ProfileTests;
    class ColorSchemeTests;
    class KeyBindingsTests;
};
namespace TerminalAppUnitTests
{
    class DynamicProfileTests;
    class JsonTests;
};

// GUID used for generating GUIDs at runtime, for profiles that did not have a
// GUID specified manually.
constexpr GUID RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID = { 0xf65ddb7e, 0x706b, 0x4499, { 0x8a, 0x50, 0x40, 0x31, 0x3c, 0xaf, 0x51, 0x0a } };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Profile : ProfileT<Profile>, IInheritable<Profile>
    {
    public:
        Profile();
        Profile(guid guid);
        static com_ptr<Profile> CloneInheritanceGraph(com_ptr<Profile> oldProfile, com_ptr<Profile> newProfile, std::unordered_map<void*, com_ptr<Profile>>& visited);
        static com_ptr<Profile> CopySettings(com_ptr<Profile> source);

        Json::Value GenerateStub() const;
        static com_ptr<Profile> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);
        static bool IsDynamicProfileObject(const Json::Value& json);

        hstring EvaluatedStartingDirectory() const;
        hstring ExpandedBackgroundImagePath() const;
        void GenerateGuidIfNecessary() noexcept;
        static guid GetGuidOrGenerateForJson(const Json::Value& json) noexcept;

        // BackgroundImageAlignment is 1 setting saved as 2 separate values
        bool HasBackgroundImageAlignment() const noexcept;
        void ClearBackgroundImageAlignment() noexcept;
        const Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept;
        void BackgroundImageHorizontalAlignment(const Windows::UI::Xaml::HorizontalAlignment& value) noexcept;
        const Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept;
        void BackgroundImageVerticalAlignment(const Windows::UI::Xaml::VerticalAlignment& value) noexcept;

        GETSET_SETTING(guid, Guid, _GenerateGuidForProfile(Name(), Source()));
        GETSET_SETTING(hstring, Name, L"Default");
        GETSET_SETTING(hstring, Source);
        GETSET_SETTING(bool, Hidden, false);
        GETSET_SETTING(guid, ConnectionType);

        GETSET_SETTING(hstring, Icon);

        GETSET_SETTING(CloseOnExitMode, CloseOnExit, CloseOnExitMode::Graceful);
        GETSET_SETTING(hstring, TabTitle);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, TabColor, nullptr);
        GETSET_SETTING(bool, SuppressApplicationTitle, false);

        GETSET_SETTING(bool, UseAcrylic, false);
        GETSET_SETTING(double, AcrylicOpacity, 0.5);
        GETSET_SETTING(Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState, Microsoft::Terminal::TerminalControl::ScrollbarState::Visible);

        GETSET_SETTING(hstring, FontFace, DEFAULT_FONT_FACE);
        GETSET_SETTING(int32_t, FontSize, DEFAULT_FONT_SIZE);
        GETSET_SETTING(Windows::UI::Text::FontWeight, FontWeight, DEFAULT_FONT_WEIGHT);
        GETSET_SETTING(hstring, Padding, DEFAULT_PADDING);

        GETSET_SETTING(hstring, Commandline, L"cmd.exe");
        GETSET_SETTING(hstring, StartingDirectory);

        GETSET_SETTING(hstring, BackgroundImagePath);
        GETSET_SETTING(double, BackgroundImageOpacity, 1.0);
        GETSET_SETTING(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::Fill);

        GETSET_SETTING(Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::TerminalControl::TextAntialiasingMode::Grayscale);
        GETSET_SETTING(bool, RetroTerminalEffect, false);
        GETSET_SETTING(bool, ForceFullRepaintRendering, false);
        GETSET_SETTING(bool, SoftwareRendering, false);

        GETSET_SETTING(hstring, ColorSchemeName, L"Campbell");

        GETSET_NULLABLE_SETTING(Windows::UI::Color, Foreground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, Background, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, SelectionBackground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, CursorColor, nullptr);

        GETSET_SETTING(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        GETSET_SETTING(bool, SnapOnInput, true);
        GETSET_SETTING(bool, AltGrAliasing, true);

        GETSET_SETTING(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        GETSET_SETTING(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);

        GETSET_SETTING(Model::BellStyle, BellStyle, BellStyle::Audible);

    private:
        std::optional<std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment>> _BackgroundImageAlignment{ std::nullopt };
        std::optional<std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment>> _getBackgroundImageAlignmentImpl() const
        {
            /*return user set value*/
            if (_BackgroundImageAlignment)
            {
                return _BackgroundImageAlignment;
            }

            /*user set value was not set*/ /*iterate through parents to find a value*/
            for (auto parent : _parents)
            {
                if (auto val{ parent->_getBackgroundImageAlignmentImpl() })
                {
                    return val;
                }
            }

            /*no value was found*/
            return std::nullopt;
        };

        static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

        static guid _GenerateGuidForProfile(const hstring& name, const hstring& source) noexcept;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::ProfileTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class TerminalAppUnitTests::DynamicProfileTests;
        friend class TerminalAppUnitTests::JsonTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(Profile);
}
