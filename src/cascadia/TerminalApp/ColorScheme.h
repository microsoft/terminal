/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ColorScheme.hpp

Abstract:
- A color scheme is a single set of colors to use as the terminal colors. These
    schemes are named, and can be used to quickly change all the colors of the
    terminal to another scheme.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once
#include <winrt/Microsoft.Terminal.Settings.h>
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "../../inc/conattrs.hpp"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace TerminalApp
{
    class ColorScheme;
};

class TerminalApp::ColorScheme
{
public:
    ColorScheme();
    ColorScheme(std::wstring name, COLORREF defaultFg, COLORREF defaultBg);
    ~ColorScheme();

    void ApplyScheme(winrt::Microsoft::Terminal::Settings::TerminalSettings terminalSettings) const;

    Json::Value ToJson() const;
    static ColorScheme FromJson(const Json::Value& json);
    bool ShouldBeLayered(const Json::Value& json) const;
    void LayerJson(const Json::Value& json);

    std::wstring_view GetName() const noexcept;
    std::array<COLORREF, COLOR_TABLE_SIZE>& GetTable() noexcept;
    COLORREF GetForeground() const noexcept;
    COLORREF GetBackground() const noexcept;
    COLORREF GetSelectionBackground() const noexcept;

    static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

private:
    std::wstring _schemeName;
    std::array<COLORREF, COLOR_TABLE_SIZE> _table;
    COLORREF _defaultForeground;
    COLORREF _defaultBackground;
    COLORREF _selectionBackground;

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
};
