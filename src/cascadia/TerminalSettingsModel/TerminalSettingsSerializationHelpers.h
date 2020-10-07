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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::TerminalControl::CursorStyle)
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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::TerminalControl::ScrollbarState)
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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode)
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ "grayscale", ValueType::Grayscale },
        pair_type{ "cleartype", ValueType::Cleartype },
        pair_type{ "aliased", ValueType::Aliased }
    };
};

// Type Description:
// - Helper for converting a user-specified closeOnExit value to its corresponding enum
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "always", ValueType::Always },
        pair_type{ "graceful", ValueType::Graceful },
        pair_type{ "never", ValueType::Never },
    };

    // Override mapping parser to add boolean parsing
    ::winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode FromJson(const Json::Value& json)
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

    using EnumMapper::TypeDescription;
};

// This specialization isn't using JSON_ENUM_MAPPER because we need to have a different
// value type (unsinged int) and return type (FontWeight struct). JSON_ENUM_MAPPER
// expects that the value type _is_ the return type.
template<>
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<::winrt::Windows::UI::Text::FontWeight> :
    public ::Microsoft::Terminal::Settings::Model::JsonUtils::EnumMapper<
        unsigned int,
        ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<::winrt::Windows::UI::Text::FontWeight>>
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

    using EnumMapper::TypeDescription;
};

JSON_ENUM_MAPPER(::winrt::Windows::UI::Xaml::ElementTheme)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "system", ValueType::Default },
        pair_type{ "light", ValueType::Light },
        pair_type{ "dark", ValueType::Dark },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::LaunchMode)
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

JSON_ENUM_MAPPER(winrt::Microsoft::Terminal::Settings::Model::ExpandCommandType)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "profiles", ValueType::Profiles },
        pair_type{ "schemes", ValueType::ColorSchemes },
    };
};

JSON_FLAG_MAPPER(::winrt::Microsoft::Terminal::TerminalControl::CopyFormat)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "none", AllClear },
        pair_type{ "html", ValueType::HTML },
        pair_type{ "rtf", ValueType::RTF },
        pair_type{ "all", AllSet },
    };

    auto FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? AllSet : AllClear;
        }
        return BaseFlagMapper::FromJson(json);
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseFlagMapper::CanConvert(json) || json.isBool();
    }
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
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition>
{
    ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition FromJson(const Json::Value& json)
    {
        ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition ret;
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
                int64_t position = std::stol(token);
                if (initialPosIndex == 0)
                {
                    ret.X = position;
                }

                if (initialPosIndex == 1)
                {
                    ret.Y = position;
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

    Json::Value ToJson(const ::winrt::Microsoft::Terminal::Settings::Model::LaunchPosition& val)
    {
        std::stringstream ss;
        if (val.X)
        {
            ss << val.X.Value();
        }
        ss << ",";
        if (val.Y)
        {
            ss << val.Y.Value();
        }
        return ss.str();
    }

    std::string TypeDescription() const
    {
        return "x, y";
    }
};

// Possible Direction values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::Direction)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down },
    };
};

// Possible SplitState values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::SplitState)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "vertical", ValueType::Vertical },
        pair_type{ "horizontal", ValueType::Horizontal },
        pair_type{ "auto", ValueType::Automatic },
    };
};

// Possible SplitType values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::SplitType)
{
    JSON_MAPPINGS(1) = {
        pair_type{ "duplicate", ValueType::Duplicate },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::SettingsTarget)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "settingsFile", ValueType::SettingsFile },
        pair_type{ "defaultsFile", ValueType::DefaultsFile },
        pair_type{ "allFiles", ValueType::AllFiles },
    };
};

JSON_ENUM_MAPPER(::winrt::Windows::System::VirtualKey)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "ctrl", ValueType::Control },
        pair_type{ "alt", ValueType::Menu },
        pair_type{ "shift", ValueType::Shift },
    };
};
