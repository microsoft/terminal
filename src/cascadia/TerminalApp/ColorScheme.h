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
#include "../../inc/conattrs.hpp"
#include "inc/cppwinrt_utils.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "ColorScheme.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace winrt::TerminalApp::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
    public:
        ColorScheme();
        ColorScheme(hstring name, Windows::UI::Color defaultFg, Windows::UI::Color defaultBg, Windows::UI::Color cursorColor);
        ~ColorScheme();

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);

        hstring Name() const noexcept;
        com_array<Windows::UI::Color> Table() const noexcept;
        Windows::UI::Color Foreground() const noexcept;
        Windows::UI::Color Background() const noexcept;
        Windows::UI::Color SelectionBackground() const noexcept;
        Windows::UI::Color CursorColor() const noexcept;

        static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

    private:
        hstring _schemeName;
        std::array<til::color, COLOR_TABLE_SIZE> _table;
        til::color _defaultForeground;
        til::color _defaultBackground;
        til::color _selectionBackground;
        til::color _cursorColor;

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ColorSchemeTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
