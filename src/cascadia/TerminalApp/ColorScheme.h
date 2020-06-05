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
    ColorScheme(std::wstring name, til::color defaultFg, til::color defaultBg, til::color cursorColor);
    ~ColorScheme();

    void ApplyScheme(winrt::Microsoft::Terminal::Settings::TerminalSettings terminalSettings) const;

    static ColorScheme FromJson(const Json::Value& json);
    bool ShouldBeLayered(const Json::Value& json) const;
    void LayerJson(const Json::Value& json);

    std::wstring_view GetName() const noexcept;
    std::array<til::color, COLOR_TABLE_SIZE>& GetTable() noexcept;
    til::color GetForeground() const noexcept;
    til::color GetBackground() const noexcept;
    til::color GetSelectionBackground() const noexcept;
    til::color GetCursorColor() const noexcept;

    static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

private:
    std::wstring _schemeName;
    std::array<til::color, COLOR_TABLE_SIZE> _table;
    til::color _defaultForeground;
    til::color _defaultBackground;
    til::color _selectionBackground;
    til::color _cursorColor;

    friend class TerminalAppLocalTests::SettingsTests;
    friend class TerminalAppLocalTests::ColorSchemeTests;
};
