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

#include "ColorScheme.g.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
    public:
        ColorScheme();
        ColorScheme(hstring name);
        ColorScheme(hstring name, COLORREF defaultFg, COLORREF defaultBg, COLORREF cursorColor);
        com_ptr<ColorScheme> Copy() const;

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);

        Json::Value ToJson() const;

        static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

        com_array<Windows::UI::Color> Table() const noexcept;
        void SetColorTableEntry(uint8_t index, const winrt::Windows::UI::Color& value) noexcept;

        GETSET_PROPERTY(winrt::hstring, Name);
        GETSET_COLORPROPERTY(Foreground); // defined in constructor
        GETSET_COLORPROPERTY(Background); // defined in constructor
        GETSET_COLORPROPERTY(SelectionBackground); // defined in constructor
        GETSET_COLORPROPERTY(CursorColor); // defined in constructor

    private:
        std::array<til::color, COLOR_TABLE_SIZE> _table;

        friend class SettingsModelLocalTests::SettingsTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
