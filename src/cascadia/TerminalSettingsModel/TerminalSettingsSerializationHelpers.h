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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Core::CursorStyle)
{
    static constexpr std::array<pair_type, 6> mappings = {
        pair_type{ "bar", ValueType::Bar },
        pair_type{ "vintage", ValueType::Vintage },
        pair_type{ "underscore", ValueType::Underscore },
        pair_type{ "doubleUnderscore", ValueType::DoubleUnderscore },
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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Control::ScrollbarState)
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ "visible", ValueType::Visible },
        pair_type{ "hidden", ValueType::Hidden }
    };
};

JSON_FLAG_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::BellStyle)
{
    static constexpr std::array<pair_type, 6> mappings = {
        pair_type{ "none", AllClear },
        pair_type{ "audible", ValueType::Audible },
        pair_type{ "visual", ValueType::Window | ValueType::Taskbar },
        pair_type{ "window", ValueType::Window },
        pair_type{ "taskbar", ValueType::Taskbar },
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

    Json::Value ToJson(const ::winrt::Microsoft::Terminal::Settings::Model::BellStyle& bellStyle)
    {
        return BaseFlagMapper::ToJson(bellStyle);
    }
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::ConvergedAlignment)
{
    // reduce repetition
    static constexpr std::array<pair_type, 9> mappings = {
        pair_type{ "center", ValueType::Horizontal_Center | ValueType::Vertical_Center },
        pair_type{ "topLeft", ValueType::Horizontal_Left | ValueType::Vertical_Top },
        pair_type{ "bottomLeft", ValueType::Horizontal_Left | ValueType::Vertical_Bottom },
        pair_type{ "left", ValueType::Horizontal_Left | ValueType::Vertical_Center },
        pair_type{ "topRight", ValueType::Horizontal_Right | ValueType::Vertical_Top },
        pair_type{ "bottomRight", ValueType::Horizontal_Right | ValueType::Vertical_Bottom },
        pair_type{ "right", ValueType::Horizontal_Right | ValueType::Vertical_Center },
        pair_type{ "top", ValueType::Horizontal_Center | ValueType::Vertical_Top },
        pair_type{ "bottom", ValueType::Horizontal_Center | ValueType::Vertical_Bottom }
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Control::TextAntialiasingMode)
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
        uint16_t,
        ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<::winrt::Windows::UI::Text::FontWeight>>
{
    // The original parser used the font weight getters Bold(), Normal(), etc.
    // They were both cumbersome and *not constant expressions*
    JSON_MAPPINGS(11) = {
        pair_type{ "thin", static_cast<uint16_t>(100u) },
        pair_type{ "extra-light", static_cast<uint16_t>(200u) },
        pair_type{ "light", static_cast<uint16_t>(300u) },
        pair_type{ "semi-light", static_cast<uint16_t>(350u) },
        pair_type{ "normal", static_cast<uint16_t>(400u) },
        pair_type{ "medium", static_cast<uint16_t>(500u) },
        pair_type{ "semi-bold", static_cast<uint16_t>(600u) },
        pair_type{ "bold", static_cast<uint16_t>(700u) },
        pair_type{ "extra-bold", static_cast<uint16_t>(800u) },
        pair_type{ "black", static_cast<uint16_t>(900u) },
        pair_type{ "extra-black", static_cast<uint16_t>(950u) },
    };

    // Override mapping parser to add unsigned int parsing
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

    Json::Value ToJson(const ::winrt::Windows::UI::Text::FontWeight& val)
    {
        const auto weight{ val.Weight };
        try
        {
            return BaseEnumMapper::ToJson(weight);
        }
        catch (SerializationError&)
        {
            return weight;
        }
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseEnumMapper::CanConvert(json) || json.isUInt();
    }

    std::string TypeDescription() const
    {
        return EnumMapper::TypeDescription() + " or number";
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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::LaunchMode)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "default", ValueType::DefaultMode },
        pair_type{ "maximized", ValueType::MaximizedMode },
        pair_type{ "fullscreen", ValueType::FullscreenMode },
        pair_type{ "focus", ValueType::FocusMode },
        pair_type{ "maximizedFocus", ValueType::MaximizedFocusMode },
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

JSON_FLAG_MAPPER(::winrt::Microsoft::Terminal::Control::CopyFormat)
{
    JSON_MAPPINGS(4) = {
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

// Possible FocusDirection values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::FocusDirection)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down },
        pair_type{ "previous", ValueType::Previous },
    };
};

// Possible ResizeDirection values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::ResizeDirection)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down }
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
    JSON_MAPPINGS(4) = {
        pair_type{ "settingsFile", ValueType::SettingsFile },
        pair_type{ "defaultsFile", ValueType::DefaultsFile },
        pair_type{ "allFiles", ValueType::AllFiles },
        pair_type{ "settingsUI", ValueType::SettingsUI },
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

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::TabSwitcherMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "mru", ValueType::MostRecentlyUsed },
        pair_type{ "inOrder", ValueType::InOrder },
        pair_type{ "disabled", ValueType::Disabled },
    };

    auto FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? ValueType::MostRecentlyUsed : ValueType::Disabled;
        }
        return BaseEnumMapper::FromJson(json);
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseEnumMapper::CanConvert(json) || json.isBool();
    }
};

// Possible Direction values
JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::MoveTabDirection)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "forward", ValueType::Forward },
        pair_type{ "backward", ValueType::Backward },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::CommandPaletteLaunchMode)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "action", ValueType::Action },
        pair_type{ "commandLine", ValueType::CommandLine },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::FindMatchDirection)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "next", ValueType::Next },
        pair_type{ "prev", ValueType::Previous },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::WindowingMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "useNew", ValueType::UseNew },
        pair_type{ "useAnyExisting", ValueType::UseAnyExisting },
        pair_type{ "useExisting", ValueType::UseExisting },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::DesktopBehavior)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "any", ValueType::Any },
        pair_type{ "toCurrent", ValueType::ToCurrent },
        pair_type{ "onCurrent", ValueType::OnCurrent },
    };
};

JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::MonitorBehavior)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "any", ValueType::Any },
        pair_type{ "toCurrent", ValueType::ToCurrent },
        pair_type{ "toMouse", ValueType::ToMouse },
    };
};
