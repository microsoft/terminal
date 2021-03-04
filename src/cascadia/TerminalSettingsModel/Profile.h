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

        hstring ToString()
        {
            return Name();
        }

        static com_ptr<Profile> CloneInheritanceGraph(com_ptr<Profile> oldProfile, com_ptr<Profile> newProfile, std::unordered_map<void*, com_ptr<Profile>>& visited);
        static com_ptr<Profile> CopySettings(com_ptr<Profile> source);

        Json::Value GenerateStub() const;
        static com_ptr<Profile> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);
        static bool IsDynamicProfileObject(const Json::Value& json);
        Json::Value ToJson() const;

        hstring EvaluatedStartingDirectory() const;
        hstring ExpandedBackgroundImagePath() const;
        static guid GetGuidOrGenerateForJson(const Json::Value& json) noexcept;

        GETSET_PROPERTY(OriginTag, Origin, OriginTag::Custom);

        SETTING(Model::Profile, guid, Guid, _GenerateGuidForProfile(Name(), Source()));
        SETTING(Model::Profile, hstring, Name, L"Default");
        SETTING(Model::Profile, hstring, Source);
        SETTING(Model::Profile, bool, Hidden, false);
        SETTING(Model::Profile, guid, ConnectionType);

        // Default Icon: Segoe MDL2 CommandPrompt icon
        SETTING(Model::Profile, hstring, Icon, L"\uE756");

        SETTING(Model::Profile, CloseOnExitMode, CloseOnExit, CloseOnExitMode::Graceful);
        SETTING(Model::Profile, hstring, TabTitle);
        NULLABLE_SETTING(Model::Profile, Windows::UI::Color, TabColor, nullptr);
        SETTING(Model::Profile, bool, SuppressApplicationTitle, false);

        SETTING(Model::Profile, bool, UseAcrylic, false);
        SETTING(Model::Profile, double, AcrylicOpacity, 0.5);
        SETTING(Model::Profile, Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState, Microsoft::Terminal::TerminalControl::ScrollbarState::Visible);

        SETTING(Model::Profile, hstring, FontFace, DEFAULT_FONT_FACE);
        SETTING(Model::Profile, int32_t, FontSize, DEFAULT_FONT_SIZE);
        SETTING(Model::Profile, Windows::UI::Text::FontWeight, FontWeight, DEFAULT_FONT_WEIGHT);
        SETTING(Model::Profile, hstring, Padding, DEFAULT_PADDING);

        SETTING(Model::Profile, hstring, Commandline, L"cmd.exe");
        SETTING(Model::Profile, hstring, StartingDirectory);

        SETTING(Model::Profile, hstring, BackgroundImagePath);
        SETTING(Model::Profile, double, BackgroundImageOpacity, 1.0);
        SETTING(Model::Profile, Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::UniformToFill);
        SETTING(Model::Profile, ConvergedAlignment, BackgroundImageAlignment, ConvergedAlignment::Horizontal_Center | ConvergedAlignment::Vertical_Center);

        SETTING(Model::Profile, Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::TerminalControl::TextAntialiasingMode::Grayscale);
        SETTING(Model::Profile, bool, RetroTerminalEffect, false);
        SETTING(Model::Profile, hstring, PixelShaderPath, L"");
        SETTING(Model::Profile, bool, ForceFullRepaintRendering, false);
        SETTING(Model::Profile, bool, SoftwareRendering, false);

        SETTING(Model::Profile, hstring, ColorSchemeName, L"Campbell");

        NULLABLE_SETTING(Model::Profile, Windows::UI::Color, Foreground, nullptr);
        NULLABLE_SETTING(Model::Profile, Windows::UI::Color, Background, nullptr);
        NULLABLE_SETTING(Model::Profile, Windows::UI::Color, SelectionBackground, nullptr);
        NULLABLE_SETTING(Model::Profile, Windows::UI::Color, CursorColor, nullptr);

        SETTING(Model::Profile, int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        SETTING(Model::Profile, bool, SnapOnInput, true);
        SETTING(Model::Profile, bool, AltGrAliasing, true);

        SETTING(Model::Profile, Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        SETTING(Model::Profile, uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);

        SETTING(Model::Profile, Model::BellStyle, BellStyle, BellStyle::Audible);

    private:
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
