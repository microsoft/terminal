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
#include "JsonUtils.h"
#include "SettingsWriteNotifier.h"

#include "ColorScheme.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>, WriteNotifiable
    {
        // A ColorScheme constructed with uninitialized_t
        // leaves _json empty to be populated lazily.
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

        com_array<Core::Color> Table() const;
        void SetColorTableEntry(uint8_t index, const Core::Color& value);

        bool IsEquivalentForSettingsMergePurposes(const winrt::com_ptr<ColorScheme>& other) noexcept;

        winrt::hstring Name() const;
        void Name(const winrt::hstring& value);

        Core::Color Foreground() const;
        void Foreground(const Core::Color& value);

        Core::Color Background() const;
        void Background(const Core::Color& value);

        Core::Color SelectionBackground() const;
        void SelectionBackground(const Core::Color& value);

        Core::Color CursorColor() const;
        void CursorColor(const Core::Color& value);

        WINRT_PROPERTY(OriginTag, Origin, OriginTag::None);

    private:
        bool _layerJson(const Json::Value& json);
        void _normalizeTableAliasKeys();

        Core::Color _getColor(std::string_view key, const Core::Color& defaultValue) const;
        void _setColor(std::string_view key, const Core::Color& value);

        Json::Value _json{ Json::ValueType::objectValue };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
