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

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        OBSERVABLE_GETSET_PROPERTY(hstring, Name, _PropertyChangedHandlers, L"Default");
        OBSERVABLE_GETSET_PROPERTY(hstring, Source, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, Hidden, _PropertyChangedHandlers, false);

        OBSERVABLE_GETSET_PROPERTY(hstring, Icon, _PropertyChangedHandlers);

        OBSERVABLE_GETSET_PROPERTY(CloseOnExitMode, CloseOnExit, _PropertyChangedHandlers, CloseOnExitMode::Graceful);
        OBSERVABLE_GETSET_PROPERTY(hstring, TabTitle, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, TabColor, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, SuppressApplicationTitle, _PropertyChangedHandlers, false);

        OBSERVABLE_GETSET_PROPERTY(bool, UseAcrylic, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(double, AcrylicOpacity, _PropertyChangedHandlers, 0.5);
        OBSERVABLE_GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::ScrollbarState, ScrollState, _PropertyChangedHandlers, Microsoft::Terminal::TerminalControl::ScrollbarState::Visible);

        OBSERVABLE_GETSET_PROPERTY(hstring, FontFace, _PropertyChangedHandlers, DEFAULT_FONT_FACE);
        OBSERVABLE_GETSET_PROPERTY(int32_t, FontSize, _PropertyChangedHandlers, DEFAULT_FONT_SIZE);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Text::FontWeight, FontWeight, _PropertyChangedHandlers, DEFAULT_FONT_WEIGHT);
        OBSERVABLE_GETSET_PROPERTY(hstring, Padding, _PropertyChangedHandlers, DEFAULT_PADDING);

        OBSERVABLE_GETSET_PROPERTY(hstring, Commandline, _PropertyChangedHandlers, L"cmd.exe");
        OBSERVABLE_GETSET_PROPERTY(hstring, StartingDirectory, _PropertyChangedHandlers);

        OBSERVABLE_GETSET_PROPERTY(hstring, BackgroundImagePath, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(double, BackgroundImageOpacity, _PropertyChangedHandlers, 1.0);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, _PropertyChangedHandlers, Windows::UI::Xaml::Media::Stretch::Fill);

        OBSERVABLE_GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::TextAntialiasingMode, AntialiasingMode, _PropertyChangedHandlers, Microsoft::Terminal::TerminalControl::TextAntialiasingMode::Grayscale);
        OBSERVABLE_GETSET_PROPERTY(bool, RetroTerminalEffect, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceFullRepaintRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, SoftwareRendering, _PropertyChangedHandlers, false);

        OBSERVABLE_GETSET_PROPERTY(hstring, ColorSchemeName, _PropertyChangedHandlers, L"Campbell");
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, Foreground, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, Background, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, SelectionBackground, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::IReference<Windows::UI::Color>, CursorColor, _PropertyChangedHandlers);

        OBSERVABLE_GETSET_PROPERTY(int32_t, HistorySize, _PropertyChangedHandlers, DEFAULT_HISTORY_SIZE);
        OBSERVABLE_GETSET_PROPERTY(bool, SnapOnInput, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, AltGrAliasing, _PropertyChangedHandlers, true);

        OBSERVABLE_GETSET_PROPERTY(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, _PropertyChangedHandlers, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, CursorHeight, _PropertyChangedHandlers, DEFAULT_CURSOR_HEIGHT);

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
