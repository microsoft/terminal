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

namespace TerminalApp
{
    class Profile;
};

class TerminalApp::Profile final
{
public:
    Profile(const winrt::guid& guid);
    Profile();

    ~Profile();

    winrt::Microsoft::Terminal::Settings::TerminalSettings CreateTerminalSettings(const std::vector<::TerminalApp::ColorScheme>& schemes) const;

    Json::Value ToJson() const;
    static Profile FromJson(const Json::Value& json);

    GUID GetGuid() const noexcept;
    std::wstring_view GetName() const noexcept;
    bool HasTabTitle() const noexcept;
    std::wstring_view GetTabTitle() const noexcept;

    void SetFontFace(std::wstring fontFace) noexcept;
    void SetColorScheme(std::optional<std::wstring> schemeName) noexcept;
    void SetTabTitle(std::wstring tabTitle) noexcept;
    void SetAcrylicOpacity(double opacity) noexcept;
    void SetCommandline(std::wstring cmdline) noexcept;
    void SetStartingDirectory(std::wstring startingDirectory) noexcept;
    void SetName(std::wstring name) noexcept;
    void SetUseAcrylic(bool useAcrylic) noexcept;
    void SetDefaultForeground(COLORREF defaultForeground) noexcept;
    void SetDefaultBackground(COLORREF defaultBackground) noexcept;

    bool HasIcon() const noexcept;
    std::wstring_view GetIconPath() const noexcept;
    void SetIconPath(std::wstring_view path) noexcept;

    bool GetCloseOnExit() const noexcept;

private:
    static std::wstring EvaluateStartingDirectory(const std::wstring& directory);

    static winrt::Microsoft::Terminal::Settings::ScrollbarState ParseScrollbarState(const std::wstring& scrollbarState);
    static winrt::Windows::UI::Xaml::Media::Stretch ParseImageStretchMode(const std::string_view imageStretchMode);
    static std::string_view SerializeImageStretchMode(const winrt::Windows::UI::Xaml::Media::Stretch imageStretchMode);
    static winrt::Microsoft::Terminal::Settings::CursorStyle _ParseCursorShape(const std::wstring& cursorShapeString);
    static std::wstring_view _SerializeCursorStyle(const winrt::Microsoft::Terminal::Settings::CursorStyle cursorShape);

    GUID _guid;
    std::wstring _name;

    // If this is set, then our colors should come from the associated color scheme
    std::optional<std::wstring> _schemeName;

    std::optional<uint32_t> _defaultForeground;
    std::optional<uint32_t> _defaultBackground;
    std::array<uint32_t, COLOR_TABLE_SIZE> _colorTable;
    std::optional<std::wstring> _tabTitle;
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

    std::optional<std::wstring> _scrollbarState;
    bool _closeOnExit;
    std::wstring _padding;

    std::optional<std::wstring> _icon;
};
