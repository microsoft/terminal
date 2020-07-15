/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TerminalSettingsSerializationHelpers.h

Abstract:
- Specializations of the JsonUtils helpers for things that might end up in a
  settings document.

--*/

#pragma once

#include "pch.h"

#include "JsonUtils.h"
#include "SettingsTypes.h"

#include <winrt/Microsoft.Terminal.Settings.h>
#include <winrt/TerminalApp.h>

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::CursorStyle)
{
    static constexpr std::array<pair_type, 5> mappings = {
        pair_type{ "bar", ValueType::Bar },
        pair_type{ "vintage", ValueType::Vintage },
        pair_type{ "underscore", ValueType::Underscore },
        pair_type{ "filledBox", ValueType::FilledBox },
        pair_type{ "emptyBox", ValueType::EmptyBox }
    };
};

JSON_ENUM_MAPPER(::winrt::Windows::UI::Xaml::Media::Stretch)
{
    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ "uniformToFill", ValueType::UniformToFill },
        pair_type{ "none", ValueType::None },
        pair_type{ "fill", ValueType::Fill },
        pair_type{ "uniform", ValueType::Uniform }
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::ScrollbarState)
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ "visible", ValueType::Visible },
        pair_type{ "hidden", ValueType::Hidden }
    };
};

JSON_ENUM_MAPPER(std::tuple<::winrt::Windows::UI::Xaml::HorizontalAlignment, ::winrt::Windows::UI::Xaml::VerticalAlignment>)
{
    // reduce repetition
    using HA = ::winrt::Windows::UI::Xaml::HorizontalAlignment;
    using VA = ::winrt::Windows::UI::Xaml::VerticalAlignment;
    static constexpr std::array<pair_type, 9> mappings = {
        pair_type{ "center", std::make_tuple(HA::Center, VA::Center) },
        pair_type{ "topLeft", std::make_tuple(HA::Left, VA::Top) },
        pair_type{ "bottomLeft", std::make_tuple(HA::Left, VA::Bottom) },
        pair_type{ "left", std::make_tuple(HA::Left, VA::Center) },
        pair_type{ "topRight", std::make_tuple(HA::Right, VA::Top) },
        pair_type{ "bottomRight", std::make_tuple(HA::Right, VA::Bottom) },
        pair_type{ "right", std::make_tuple(HA::Right, VA::Center) },
        pair_type{ "top", std::make_tuple(HA::Center, VA::Top) },
        pair_type{ "bottom", std::make_tuple(HA::Center, VA::Bottom) }
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::TextAntialiasingMode)
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ "grayscale", ValueType::Grayscale },
        pair_type{ "cleartype", ValueType::Cleartype },
        pair_type{ "aliased", ValueType::Aliased }
    };
};

// Type Description:
// - Helper for converting a user-specified closeOnExit value to its corresponding enum
JSON_ENUM_MAPPER(::TerminalApp::CloseOnExitMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "always", ValueType::Always },
        pair_type{ "graceful", ValueType::Graceful },
        pair_type{ "never", ValueType::Never },
    };

    // Override mapping parser to add boolean parsing
    CloseOnExitMode FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? ValueType::Graceful : ValueType::Never;
        }
        return EnumMapper::FromJson(json);
    }

    bool CanConvert(const Json::Value& json)
    {
        return EnumMapper::CanConvert(json) || json.isBool();
    }
};

// This specialization isn't using JSON_ENUM_MAPPER because we need to have a different
// value type (unsinged int) and return type (FontWeight struct). JSON_ENUM_MAPPER
// expects that the value type _is_ the return type.
template<>
struct ::TerminalApp::JsonUtils::ConversionTrait<::winrt::Windows::UI::Text::FontWeight> :
    public ::TerminalApp::JsonUtils::EnumMapper<
        unsigned int,
        ::TerminalApp::JsonUtils::ConversionTrait<::winrt::Windows::UI::Text::FontWeight>>
{
    // The original parser used the font weight getters Bold(), Normal(), etc.
    // They were both cumbersome and *not constant expressions*
    JSON_MAPPINGS(11) = {
        pair_type{ "thin", 100u },
        pair_type{ "extra-light", 200u },
        pair_type{ "light", 300u },
        pair_type{ "semi-light", 350u },
        pair_type{ "normal", 400u },
        pair_type{ "medium", 500u },
        pair_type{ "semi-bold", 600u },
        pair_type{ "bold", 700u },
        pair_type{ "extra-bold", 800u },
        pair_type{ "black", 900u },
        pair_type{ "extra-black", 950u },
    };

    // Override mapping parser to add boolean parsing
    auto FromJson(const Json::Value& json)
    {
        unsigned int value{ 400 };
        if (json.isUInt())
        {
            value = json.asUInt();
        }
        else
        {
            value = BaseEnumMapper::FromJson(json);
        }

        ::winrt::Windows::UI::Text::FontWeight weight{
            static_cast<uint16_t>(std::clamp(value, 100u, 990u))
        };
        return weight;
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseEnumMapper::CanConvert(json) || json.isUInt();
    }
};

JSON_ENUM_MAPPER(::winrt::Windows::UI::Xaml::ElementTheme)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "system", ValueType::Default },
        pair_type{ "light", ValueType::Light },
        pair_type{ "dark", ValueType::Dark },
    };
};

JSON_ENUM_MAPPER(::winrt::TerminalApp::LaunchMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "default", ValueType::DefaultMode },
        pair_type{ "maximized", ValueType::MaximizedMode },
        pair_type{ "fullscreen", ValueType::FullscreenMode },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "equal", ValueType::Equal },
        pair_type{ "titleLength", ValueType::SizeToContent },
        pair_type{ "compact", ValueType::Compact },
    };
};

// Type Description:
// - Helper for converting the initial position string into
//   2 coordinate values. We allow users to only provide one coordinate,
//   thus, we use comma as the separator:
//   (100, 100): standard input string
//   (, 100), (100, ): if a value is missing, we set this value as a default
//   (,): both x and y are set to default
//   (abc, 100): if a value is not valid, we treat it as default
//   (100, 100, 100): we only read the first two values, this is equivalent to (100, 100)
template<>
struct ::TerminalApp::JsonUtils::ConversionTrait<::TerminalApp::LaunchPosition>
{
    ::TerminalApp::LaunchPosition FromJson(const Json::Value& json)
    {
        ::TerminalApp::LaunchPosition ret;
        std::string initialPosition{ json.asString() };
        static constexpr char singleCharDelim = ',';
        std::stringstream tokenStream(initialPosition);
        std::string token;
        uint8_t initialPosIndex = 0;

        // Get initial position values till we run out of delimiter separated values in the stream
        // or we hit max number of allowable values (= 2)
        // Non-numeral values or empty string will be caught as exception and we do not assign them
        for (; std::getline(tokenStream, token, singleCharDelim) && (initialPosIndex < 2); initialPosIndex++)
        {
            try
            {
                int32_t position = std::stoi(token);
                if (initialPosIndex == 0)
                {
                    ret.x.emplace(position);
                }

                if (initialPosIndex == 1)
                {
                    ret.y.emplace(position);
                }
            }
            catch (...)
            {
                // Do nothing
            }
        }
        return ret;
    }

    bool CanConvert(const Json::Value& json)
    {
        return json.isString();
    }
};

// Possible Direction values
JSON_ENUM_MAPPER(::winrt::TerminalApp::Direction)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down },
    };
};

// Possible SplitState values
JSON_ENUM_MAPPER(::winrt::TerminalApp::SplitState)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "vertical", ValueType::Vertical },
        pair_type{ "horizontal", ValueType::Horizontal },
        pair_type{ "auto", ValueType::Automatic },
    };
};

// Possible SplitType values
JSON_ENUM_MAPPER(::winrt::TerminalApp::SplitType)
{
    JSON_MAPPINGS(1) = {
        pair_type{ "duplicate", ValueType::Duplicate },
    };
};

JSON_ENUM_MAPPER(::winrt::TerminalApp::SettingsTarget)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "settingsFile", ValueType::SettingsFile },
        pair_type{ "defaultsFile", ValueType::DefaultsFile },
        pair_type{ "allFiles", ValueType::AllFiles },
    };
};
