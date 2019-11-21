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
#include "ColorScheme.h"

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

namespace TerminalApp
{
    class Profile;
};

class TerminalApp::Profile final
{
public:
    Profile();
    Profile(const std::optional<GUID>& guid);

    ~Profile();

    winrt::Microsoft::Terminal::Settings::TerminalSettings CreateTerminalSettings(const std::unordered_map<std::wstring, ColorScheme>& schemes) const;

    Json::Value ToJson() const;
    Json::Value DiffToJson(const Profile& other) const;
    Json::Value GenerateStub() const;
    static Profile FromJson(const Json::Value& json);
    bool ShouldBeLayered(const Json::Value& json) const;
    void LayerJson(const Json::Value& json);
    static bool IsDynamicProfileObject(const Json::Value& json);

    bool HasGuid() const noexcept;
    bool HasSource() const noexcept;
    GUID GetGuid() const noexcept;
    void SetSource(std::wstring_view sourceNamespace) noexcept;
    std::wstring_view GetName() const noexcept;
    bool HasConnectionType() const noexcept;
    GUID GetConnectionType() const noexcept;

    void SetFontFace(std::wstring fontFace) noexcept;
    void SetColorScheme(std::optional<std::wstring> schemeName) noexcept;
    std::optional<std::wstring>& GetSchemeName() noexcept;
    void SetTabTitle(std::wstring tabTitle) noexcept;
    void SetSuppressApplicationTitle(bool suppressApplicationTitle) noexcept;
    void SetAcrylicOpacity(double opacity) noexcept;
    void SetCommandline(std::wstring cmdline) noexcept;
    void SetStartingDirectory(std::wstring startingDirectory) noexcept;
    void SetName(std::wstring name) noexcept;
    void SetUseAcrylic(bool useAcrylic) noexcept;
    void SetDefaultForeground(COLORREF defaultForeground) noexcept;
    void SetDefaultBackground(COLORREF defaultBackground) noexcept;
    void SetSelectionBackground(COLORREF selectionBackground) noexcept;
    void SetCloseOnExit(bool defaultClose) noexcept;
    void SetConnectionType(GUID connectionType) noexcept;

    bool HasIcon() const noexcept;
    winrt::hstring GetExpandedIconPath() const;
    void SetIconPath(std::wstring_view path);

    bool HasBackgroundImage() const noexcept;
    winrt::hstring GetExpandedBackgroundImagePath() const;

    bool GetCloseOnExit() const noexcept;
    bool GetSuppressApplicationTitle() const noexcept;
    bool IsHidden() const noexcept;

    void GenerateGuidIfNecessary() noexcept;

    static GUID GetGuidOrGenerateForJson(const Json::Value& json) noexcept;

private:
    static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

    static winrt::Microsoft::Terminal::Settings::ScrollbarState ParseScrollbarState(const std::wstring& scrollbarState);
    static winrt::Windows::UI::Xaml::Media::Stretch ParseImageStretchMode(const std::string_view imageStretchMode);
    static winrt::Windows::UI::Xaml::Media::Stretch _ConvertJsonToStretchMode(const Json::Value& json);
    static std::string_view SerializeImageStretchMode(const winrt::Windows::UI::Xaml::Media::Stretch imageStretchMode);
    static std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment> ParseImageAlignment(const std::string_view imageAlignment);
    static std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment> _ConvertJsonToAlignment(const Json::Value& json);

    static std::string_view SerializeImageAlignment(const std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment> imageAlignment);
    static winrt::Microsoft::Terminal::Settings::CursorStyle _ParseCursorShape(const std::wstring& cursorShapeString);
    static std::wstring_view _SerializeCursorStyle(const winrt::Microsoft::Terminal::Settings::CursorStyle cursorShape);

    static GUID _GenerateGuidForProfile(const std::wstring& name, const std::optional<std::wstring>& source) noexcept;

    std::optional<GUID> _guid{ std::nullopt };
    std::optional<std::wstring> _source{ std::nullopt };
    std::wstring _name;
    std::optional<GUID> _connectionType;
    bool _hidden;

    // If this is set, then our colors should come from the associated color scheme
    std::optional<std::wstring> _schemeName;

    std::optional<uint32_t> _defaultForeground;
    std::optional<uint32_t> _defaultBackground;
    std::optional<uint32_t> _selectionBackground;
    std::array<uint32_t, COLOR_TABLE_SIZE> _colorTable;
    std::optional<std::wstring> _tabTitle;
    bool _suppressApplicationTitle;
    int32_t _historySize;
    bool _snapOnInput;
    uint32_t _cursorColor;
    uint32_t _cursorHeight;
    winrt::Microsoft::Terminal::Settings::CursorStyle _cursorShape;

    std::wstring _commandline;
    std::wstring _fontFace;
    std::optional<std::wstring> _startingDirectory;
    int32_t _fontSize;
    double _acrylicTransparency;
    bool _useAcrylic;

    std::optional<std::wstring> _backgroundImage;
    std::optional<double> _backgroundImageOpacity;
    std::optional<winrt::Windows::UI::Xaml::Media::Stretch> _backgroundImageStretchMode;
    std::optional<std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment>> _backgroundImageAlignment;

    std::optional<std::wstring> _scrollbarState;
    bool _closeOnExit;
    std::wstring _padding;

    std::optional<std::wstring> _icon;

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ProfileTests;
    friend class TerminalAppUnitTests::JsonTests;
    friend class TerminalAppUnitTests::DynamicProfileTests;
};
