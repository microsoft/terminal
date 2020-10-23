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
        ColorScheme(hstring name, Windows::UI::Color defaultFg, Windows::UI::Color defaultBg, Windows::UI::Color cursorColor);
        com_ptr<ColorScheme> Copy() const;

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);

        Json::Value ToJson();

        static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

        com_array<Windows::UI::Color> Table() const noexcept;
        void SetColorTableEntry(uint8_t index, const winrt::Windows::UI::Color& value) noexcept;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        GETSET_PROPERTY(winrt::hstring, Name);
        OBSERVABLE_GETSET_COLORPROPERTY(Foreground, _PropertyChangedHandlers); // defined in constructor
        OBSERVABLE_GETSET_COLORPROPERTY(Background, _PropertyChangedHandlers); // defined in constructor
        OBSERVABLE_GETSET_COLORPROPERTY(SelectionBackground, _PropertyChangedHandlers); // defined in constructor
        OBSERVABLE_GETSET_COLORPROPERTY(CursorColor, _PropertyChangedHandlers); // defined in constructor

    private:
        std::array<til::color, COLOR_TABLE_SIZE> _table;

        friend class SettingsModelLocalTests::SettingsTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}
