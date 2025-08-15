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
#include "DefaultSettings.h"

#include "ColorScheme.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
        // A ColorScheme constructed with uninitialized_t
        // leaves _table uninitialized.
        struct uninitialized_t
        {
        };

    public:
        ColorScheme() noexcept;
        explicit ColorScheme(uninitialized_t) noexcept {}
        explicit ColorScheme(const winrt::hstring& name) noexcept;

        com_ptr<ColorScheme> Copy() const;

        hstring ToString()
        {
            return Name();
        }

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        Json::Value ToJson() const;

        com_array<Model::Color> Table() const noexcept;
        void SetColorTableEntry(uint8_t index, const Model::Color& value) noexcept;

        bool IsEquivalentForSettingsMergePurposes(const winrt::com_ptr<ColorScheme>& other) noexcept;

        WINRT_PROPERTY(winrt::hstring, Name);
        WINRT_PROPERTY(OriginTag, Origin, OriginTag::None);
        WINRT_PROPERTY(Model::Color, Foreground, static_cast<Model::Color>(DEFAULT_FOREGROUND));
        WINRT_PROPERTY(Model::Color, Background, static_cast<Model::Color>(DEFAULT_BACKGROUND));
        WINRT_PROPERTY(Model::Color, SelectionBackground, static_cast<Model::Color>(DEFAULT_FOREGROUND));
        WINRT_PROPERTY(Model::Color, CursorColor, static_cast<Model::Color>(DEFAULT_CURSOR_COLOR));

    private:
        bool _layerJson(const Json::Value& json);

        std::array<Model::Color, COLOR_TABLE_SIZE> _table;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
