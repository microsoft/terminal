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

// Use this macro to quick implement both the getter and setter for a color property.
// This should only be used for color types where there's no logic in the
// getter/setter beyond just accessing/updating the value.
#define WINRT_TERMINAL_COLOR_PROPERTY(name, ...)                      \
public:                                                               \
    Core::Color name() const noexcept { return _##name; }             \
    void name(const Core::Color& value) noexcept { _##name = value; } \
                                                                      \
private:                                                              \
    Core::Color _##name{ __VA_ARGS__ };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
    public:
        // This default constructor creates an instance with an
        // uninitialized color table. Use FromJson() instead.
        ColorScheme() noexcept;

        ColorScheme(hstring name);
        com_ptr<ColorScheme> Copy() const;

        hstring ToString()
        {
            return Name();
        }

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        Json::Value ToJson() const;

        const std::array<Core::Color, COLOR_TABLE_SIZE>& TableReference() const noexcept;
        com_array<Core::Color> Table() const noexcept;
        void SetColorTableEntry(uint8_t index, const Core::Color& value) noexcept;

        WINRT_PROPERTY(winrt::hstring, Name);
        WINRT_TERMINAL_COLOR_PROPERTY(Foreground); // defined in constructor
        WINRT_TERMINAL_COLOR_PROPERTY(Background); // defined in constructor
        WINRT_TERMINAL_COLOR_PROPERTY(SelectionBackground); // defined in constructor
        WINRT_TERMINAL_COLOR_PROPERTY(CursorColor); // defined in constructor

    private:
        bool LayerJson(const Json::Value& json);

        std::array<Core::Color, COLOR_TABLE_SIZE> _table;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
