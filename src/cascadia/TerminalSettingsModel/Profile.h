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

#include "../inc/cppwinrt_utils.h"
#include "JsonUtils.h"
#include <DefaultSettings.h>

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ProfileTests;
};
namespace TerminalAppUnitTests
{
    class JsonTests;
    class DynamicProfileTests;
};

// GUID used for generating GUIDs at runtime, for profiles that did not have a
// GUID specified manually.
constexpr GUID RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID = { 0xf65ddb7e, 0x706b, 0x4499, { 0x8a, 0x50, 0x40, 0x31, 0x3c, 0xaf, 0x51, 0x0a } };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Profile : ProfileT<Profile>
    {
    public:
        Profile();
        Profile(guid guid);

        Json::Value GenerateStub() const;
        static com_ptr<Profile> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);
        static bool IsDynamicProfileObject(const Json::Value& json);

        hstring EvaluatedStartingDirectory() const;
        hstring ExpandedIconPath() const;
        hstring ExpandedBackgroundImagePath() const;
        void GenerateGuidIfNecessary() noexcept;
        static guid GetGuidOrGenerateForJson(const Json::Value& json) noexcept;

        bool HasGuid() const noexcept;
        winrt::guid Guid() const;
        void Guid(const winrt::guid& guid) noexcept;

        bool HasConnectionType() const noexcept;
        winrt::guid ConnectionType() const noexcept;
        void ConnectionType(const winrt::guid& conType) noexcept;

        // BackgroundImageAlignment is 1 setting saved as 2 separate values
        const Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept;
        void BackgroundImageHorizontalAlignment(const Windows::UI::Xaml::HorizontalAlignment& value) noexcept;
        const Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept;
        void BackgroundImageVerticalAlignment(const Windows::UI::Xaml::VerticalAlignment& value) noexcept;

        GETSET_PROPERTY(hstring, Name, L"Default");
        GETSET_PROPERTY(hstring, Source);
        GETSET_PROPERTY(bool, Hidden, false);

        GETSET_PROPERTY(hstring, IconPath);

        GETSET_PROPERTY(CloseOnExitMode, CloseOnExit, CloseOnExitMode::Graceful);
        GETSET_PROPERTY(hstring, TabTitle);
        GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, TabColor);
        GETSET_PROPERTY(bool, SuppressApplicationTitle, false);

        GETSET_PROPERTY(bool, UseAcrylic, false);
        GETSET_PROPERTY(double, AcrylicOpacity, 0.5);
        GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState, Microsoft::Terminal::TerminalControl::ScrollbarState::Visible);

        GETSET_PROPERTY(hstring, FontFace, DEFAULT_FONT_FACE);
        GETSET_PROPERTY(int32_t, FontSize, DEFAULT_FONT_SIZE);
        GETSET_PROPERTY(Windows::UI::Text::FontWeight, FontWeight, DEFAULT_FONT_WEIGHT);
        GETSET_PROPERTY(hstring, Padding, DEFAULT_PADDING);

        GETSET_PROPERTY(hstring, Commandline, L"cmd.exe");
        GETSET_PROPERTY(hstring, StartingDirectory);

        GETSET_PROPERTY(hstring, BackgroundImagePath);
        GETSET_PROPERTY(double, BackgroundImageOpacity, 1.0);
        GETSET_PROPERTY(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::Fill);

        GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::TerminalControl::TextAntialiasingMode::Grayscale);
        GETSET_PROPERTY(bool, RetroTerminalEffect, false);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
        GETSET_PROPERTY(bool, SoftwareRendering, false);

        GETSET_PROPERTY(hstring, ColorSchemeName, L"Campbell");
        GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, Foreground);
        GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, Background);
        GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, SelectionBackground);
        GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, CursorColor);

        GETSET_PROPERTY(int32_t, HistorySize, DEFAULT_HISTORY_SIZE);
        GETSET_PROPERTY(bool, SnapOnInput, true);
        GETSET_PROPERTY(bool, AltGrAliasing, true);

        GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        GETSET_PROPERTY(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT);

    private:
        std::optional<winrt::guid> _Guid{ std::nullopt };
        std::optional<winrt::guid> _ConnectionType{ std::nullopt };
        std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment> _BackgroundImageAlignment{
            Windows::UI::Xaml::HorizontalAlignment::Center,
            Windows::UI::Xaml::VerticalAlignment::Center
        };

        static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

        static guid _GenerateGuidForProfile(const hstring& name, const hstring& source) noexcept;

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ProfileTests;
        friend class TerminalAppUnitTests::JsonTests;
        friend class TerminalAppUnitTests::DynamicProfileTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(Profile);
}
