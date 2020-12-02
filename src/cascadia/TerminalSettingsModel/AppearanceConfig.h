// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AppearanceConfig.g.h"
#include "JsonUtils.h"
#include "../inc/cppwinrt_utils.h"
#include "IInheritable.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig>, IInheritable<AppearanceConfig>
    {
    public:
        AppearanceConfig();

        void LayerJson(const Json::Value& json);
        winrt::hstring ExpandedBackgroundImagePath();

        // BackgroundImageAlignment is 1 setting saved as 2 separate values
        bool HasBackgroundImageAlignment() const noexcept;
        void ClearBackgroundImageAlignment() noexcept;
        const Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept;
        void BackgroundImageVerticalAlignment(const Windows::UI::Xaml::VerticalAlignment& value) noexcept;
        const Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept;
        void BackgroundImageHorizontalAlignment(const Windows::UI::Xaml::HorizontalAlignment& value) noexcept;

        GETSET_SETTING(hstring, ColorSchemeName, L"Campbell");
        GETSET_NULLABLE_SETTING(Windows::UI::Color, Foreground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, Background, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, SelectionBackground, nullptr);
        GETSET_NULLABLE_SETTING(Windows::UI::Color, CursorColor, nullptr);
        GETSET_SETTING(Microsoft::Terminal::TerminalControl::CursorStyle, CursorShape, Microsoft::Terminal::TerminalControl::CursorStyle::Bar);
        GETSET_SETTING(hstring, BackgroundImagePath);

        GETSET_SETTING(double, BackgroundImageOpacity, 1.0);
        GETSET_SETTING(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, Windows::UI::Xaml::Media::Stretch::UniformToFill);

    private:
        std::optional<std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment>> _BackgroundImageAlignment{ std::nullopt };
        std::optional<std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment>> _getBackgroundImageAlignmentImpl() const
        {
            /*return user set value*/
            if (_BackgroundImageAlignment)
            {
                return _BackgroundImageAlignment;
            }

            /*user set value was not set*/ /*iterate through parents to find a value*/
            for (auto parent : _parents)
            {
                if (auto val{ parent->_getBackgroundImageAlignmentImpl() })
                {
                    return val;
                }
            }

            /*no value was found*/
            return std::nullopt;
        };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(AppearanceConfig);
}
