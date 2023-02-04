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
#include "ModelSerializationHelpers.h"

JSON_ENUM_MAPPER(MTCore::CursorStyle)
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

// Type Description:
// - Helper for converting a user-specified adjustTextMode value to its corresponding enum
JSON_ENUM_MAPPER(MTCore::AdjustTextMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "never", ValueType::Never },
        pair_type{ "indexed", ValueType::Indexed },
        pair_type{ "always", ValueType::Always },
    };

    // Override mapping parser to add boolean parsing
    MTCore::AdjustTextMode FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? ValueType::Indexed : ValueType::Never;
        }
        return EnumMapper::FromJson(json);
    }

    bool CanConvert(const Json::Value& json)
    {
        return EnumMapper::CanConvert(json) || json.isBool();
    }

    using EnumMapper::TypeDescription;
};

JSON_ENUM_MAPPER(WUXMedia::Stretch)
{
    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ "uniformToFill", ValueType::UniformToFill },
        pair_type{ "none", ValueType::None },
        pair_type{ "fill", ValueType::Fill },
        pair_type{ "uniform", ValueType::Uniform }
    };
};

JSON_ENUM_MAPPER(MTControl::ScrollbarState)
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ "visible", ValueType::Visible },
        pair_type{ "hidden", ValueType::Hidden },
        pair_type{ "always", ValueType::Always }
    };
};

JSON_ENUM_MAPPER(MTCore::MatchMode)
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ "none", ValueType::None },
        pair_type{ "all", ValueType::All }
    };
};

JSON_FLAG_MAPPER(MTSM::BellStyle)
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

    Json::Value ToJson(const MTSM::BellStyle& bellStyle)
    {
        return BaseFlagMapper::ToJson(bellStyle);
    }
};

JSON_ENUM_MAPPER(MTSM::ConvergedAlignment)
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

JSON_ENUM_MAPPER(MTControl::TextAntialiasingMode)
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ "grayscale", ValueType::Grayscale },
        pair_type{ "cleartype", ValueType::Cleartype },
        pair_type{ "aliased", ValueType::Aliased }
    };
};

// Type Description:
// - Helper for converting a user-specified closeOnExit value to its corresponding enum
JSON_ENUM_MAPPER(MTSM::CloseOnExitMode)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "always", ValueType::Always },
        pair_type{ "graceful", ValueType::Graceful },
        pair_type{ "never", ValueType::Never },
        pair_type{ "automatic", ValueType::Automatic },
    };

    // Override mapping parser to add boolean parsing
    MTSM::CloseOnExitMode FromJson(const Json::Value& json)
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
// value type (unsigned int) and return type (FontWeight struct). JSON_ENUM_MAPPER
// expects that the value type _is_ the return type.
template<>
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<WUT::FontWeight> :
    public ::Microsoft::Terminal::Settings::Model::JsonUtils::EnumMapper<
        uint16_t,
        ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<WUT::FontWeight>>
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

        WUT::FontWeight weight{
            static_cast<uint16_t>(std::clamp(value, 100u, 990u))
        };
        return weight;
    }

    Json::Value ToJson(const WUT::FontWeight& val)
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

JSON_ENUM_MAPPER(WUX::ElementTheme)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "system", ValueType::Default },
        pair_type{ "light", ValueType::Light },
        pair_type{ "dark", ValueType::Dark },
    };
};

JSON_ENUM_MAPPER(MTSM::NewTabPosition)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "afterLastTab", ValueType::AfterLastTab },
        pair_type{ "afterCurrentTab", ValueType::AfterCurrentTab },
    };
};

JSON_ENUM_MAPPER(MTSM::FirstWindowPreference)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "defaultProfile", ValueType::DefaultProfile },
        pair_type{ "persistedWindowLayout", ValueType::PersistedWindowLayout },
    };
};

JSON_ENUM_MAPPER(MTSM::LaunchMode)
{
    JSON_MAPPINGS(8) = {
        pair_type{ "default", ValueType::DefaultMode },
        pair_type{ "maximized", ValueType::MaximizedMode },
        pair_type{ "fullscreen", ValueType::FullscreenMode },
        pair_type{ "maximizedFullscreen", ValueType::MaximizedMode | ValueType::FullscreenMode },
        pair_type{ "focus", ValueType::FocusMode },
        pair_type{ "maximizedFocus", ValueType::MaximizedFocusMode },
        pair_type{ "fullscreenFocus", ValueType::FullscreenMode | ValueType::FocusMode },
        pair_type{ "maximizedFullscreenFocus", ValueType::MaximizedMode | ValueType::FullscreenMode | ValueType::FocusMode },
    };
};

JSON_ENUM_MAPPER(MUXC::TabViewWidthMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "equal", ValueType::Equal },
        pair_type{ "titleLength", ValueType::SizeToContent },
        pair_type{ "compact", ValueType::Compact },
    };
};

JSON_ENUM_MAPPER(MTSM::ExpandCommandType)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "profiles", ValueType::Profiles },
        pair_type{ "schemes", ValueType::ColorSchemes },
    };
};

JSON_FLAG_MAPPER(MTControl::CopyFormat)
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

template<>
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<MTSM::LaunchPosition>
{
    MTSM::LaunchPosition FromJson(const Json::Value& json)
    {
        return LaunchPositionFromString(json.asString());
    }

    bool CanConvert(const Json::Value& json)
    {
        return json.isString();
    }

    Json::Value ToJson(const MTSM::LaunchPosition& val)
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

struct IntAsFloatPercentConversionTrait : ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<double>
{
    double FromJson(const Json::Value& json)
    {
        return ::base::saturated_cast<double>(json.asUInt()) / 100.0;
    }

    bool CanConvert(const Json::Value& json)
    {
        if (!json.isUInt())
        {
            return false;
        }
        const auto value = json.asUInt();
        return value >= 0 && value <= 100;
    }

    Json::Value ToJson(const double& val)
    {
        return std::clamp(::base::saturated_cast<uint32_t>(std::round(val * 100.0)), 0u, 100u);
    }

    std::string TypeDescription() const
    {
        return "number (>= 0, <=100)";
    }
};

// Possible FocusDirection values
JSON_ENUM_MAPPER(MTSM::FocusDirection)
{
    JSON_MAPPINGS(10) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down },
        pair_type{ "previous", ValueType::Previous },
        pair_type{ "previousInOrder", ValueType::PreviousInOrder },
        pair_type{ "nextInOrder", ValueType::NextInOrder },
        pair_type{ "first", ValueType::First },
        pair_type{ "parent", ValueType::Parent },
        pair_type{ "child", ValueType::Child },
    };
};

// Possible ResizeDirection values
JSON_ENUM_MAPPER(MTSM::ResizeDirection)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "left", ValueType::Left },
        pair_type{ "right", ValueType::Right },
        pair_type{ "up", ValueType::Up },
        pair_type{ "down", ValueType::Down }
    };
};

// Possible SplitState values
JSON_ENUM_MAPPER(MTSM::SplitDirection)
{
    JSON_MAPPINGS(7) = {
        pair_type{ "auto", ValueType::Automatic },
        pair_type{ "up", ValueType::Up },
        pair_type{ "right", ValueType::Right },
        pair_type{ "down", ValueType::Down },
        pair_type{ "left", ValueType::Left },
        pair_type{ "vertical", ValueType::Right },
        pair_type{ "horizontal", ValueType::Down },
    };
};

// Possible SplitType values
JSON_ENUM_MAPPER(MTSM::SplitType)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "manual", ValueType::Manual },
        pair_type{ "duplicate", ValueType::Duplicate },
    };
};

JSON_ENUM_MAPPER(MTSM::SettingsTarget)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "settingsFile", ValueType::SettingsFile },
        pair_type{ "defaultsFile", ValueType::DefaultsFile },
        pair_type{ "allFiles", ValueType::AllFiles },
        pair_type{ "settingsUI", ValueType::SettingsUI },
    };
};

JSON_ENUM_MAPPER(WS::VirtualKey)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "ctrl", ValueType::Control },
        pair_type{ "alt", ValueType::Menu },
        pair_type{ "shift", ValueType::Shift },
    };
};

JSON_ENUM_MAPPER(MTSM::TabSwitcherMode)
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
JSON_ENUM_MAPPER(MTSM::MoveTabDirection)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "forward", ValueType::Forward },
        pair_type{ "backward", ValueType::Backward },
    };
};

JSON_ENUM_MAPPER(MTSM::CommandPaletteLaunchMode)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "action", ValueType::Action },
        pair_type{ "commandLine", ValueType::CommandLine },
    };
};

JSON_ENUM_MAPPER(MTSM::FindMatchDirection)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "next", ValueType::Next },
        pair_type{ "prev", ValueType::Previous },
    };
};

JSON_ENUM_MAPPER(MTSM::WindowingMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "useNew", ValueType::UseNew },
        pair_type{ "useAnyExisting", ValueType::UseAnyExisting },
        pair_type{ "useExisting", ValueType::UseExisting },
    };
};

JSON_ENUM_MAPPER(MTSM::DesktopBehavior)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "any", ValueType::Any },
        pair_type{ "toCurrent", ValueType::ToCurrent },
        pair_type{ "onCurrent", ValueType::OnCurrent },
    };
};

JSON_ENUM_MAPPER(MTSM::MonitorBehavior)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "any", ValueType::Any },
        pair_type{ "toCurrent", ValueType::ToCurrent },
        pair_type{ "toMouse", ValueType::ToMouse },
    };
};

JSON_ENUM_MAPPER(MTControl::ClearBufferType)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "all", ValueType::All },
        pair_type{ "screen", ValueType::Screen },
        pair_type{ "scrollback", ValueType::Scrollback },
    };
};

JSON_FLAG_MAPPER(MTSM::IntenseStyle)
{
    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ "none", AllClear },
        pair_type{ "bold", ValueType::Bold },
        pair_type{ "bright", ValueType::Bright },
        pair_type{ "all", AllSet },

    };
};

JSON_ENUM_MAPPER(MTSM::InfoBarMessage)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "closeOnExitInfo", ValueType::CloseOnExitInfo },
        pair_type{ "keyboardServiceWarning", ValueType::KeyboardServiceWarning },
        pair_type{ "setAsDefault", ValueType::SetAsDefault },
    };
};

template<>
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<MTSM::ThemeColor>
{
    static constexpr std::string_view accentString{ "accent" };
    static constexpr std::string_view terminalBackgroundString{ "terminalBackground" };

    MTSM::ThemeColor FromJson(const Json::Value& json)
    {
        if (json == Json::Value::null)
        {
            return nullptr;
        }
        const auto string{ Detail::GetStringView(json) };
        if (string == accentString)
        {
            return MTSM::ThemeColor::FromAccent();
        }
        else if (string == terminalBackgroundString)
        {
            return MTSM::ThemeColor::FromTerminalBackground();
        }
        else
        {
            return MTSM::ThemeColor::FromColor(::Microsoft::Console::Utils::ColorFromHexString(string));
        }
    }

    bool CanConvert(const Json::Value& json)
    {
        if (json == Json::Value::null)
        {
            return true;
        }
        if (!json.isString())
        {
            return false;
        }

        const auto string{ Detail::GetStringView(json) };
        const auto isColorSpec = (string.length() == 9 || string.length() == 7 || string.length() == 4) && string.front() == '#';
        const auto isAccent = string == accentString;
        const auto isTerminalBackground = string == terminalBackgroundString;
        return isColorSpec || isAccent || isTerminalBackground;
    }

    Json::Value ToJson(const MTSM::ThemeColor& val)
    {
        if (val == nullptr)
        {
            return Json::Value::null;
        }

        switch (val.ColorType())
        {
        case MTSM::ThemeColorType::Accent:
        {
            return "accent";
        }
        case MTSM::ThemeColorType::Color:
        {
            return til::u16u8(til::color{ val.Color() }.ToHexString(false));
        }
        case MTSM::ThemeColorType::TerminalBackground:
        {
            return "terminalBackground";
        }
        }
        return til::u16u8(til::color{ val.Color() }.ToHexString(false));
    }

    std::string TypeDescription() const
    {
        return "ThemeColor (#rrggbb, #rgb, #rrggbbaa, accent, terminalBackground)";
    }
};

JSON_ENUM_MAPPER(MTSM::TabCloseButtonVisibility)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "always", ValueType::Always },
        pair_type{ "hover", ValueType::Hover },
        pair_type{ "never", ValueType::Never },
    };
};

// Possible ScrollToMarkDirection values
JSON_ENUM_MAPPER(MTControl::ScrollToMarkDirection)
{
    JSON_MAPPINGS(4) = {
        pair_type{ "previous", ValueType::Previous },
        pair_type{ "next", ValueType::Next },
        pair_type{ "first", ValueType::First },
        pair_type{ "last", ValueType::Last },
    };
};

// Possible NewTabMenuEntryType values
JSON_ENUM_MAPPER(MTSM::NewTabMenuEntryType)
{
    JSON_MAPPINGS(5) = {
        pair_type{ "profile", ValueType::Profile },
        pair_type{ "separator", ValueType::Separator },
        pair_type{ "folder", ValueType::Folder },
        pair_type{ "remainingProfiles", ValueType::RemainingProfiles },
        pair_type{ "matchProfiles", ValueType::MatchProfiles },
    };
};

// Possible FolderEntryInlining values
JSON_ENUM_MAPPER(MTSM::FolderEntryInlining)
{
    JSON_MAPPINGS(2) = {
        pair_type{ "never", ValueType::Never },
        pair_type{ "auto", ValueType::Auto },
    };
};

template<>
struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<MTControl::SelectionColor>
{
    MTControl::SelectionColor FromJson(const Json::Value& json)
    {
        const auto string = Detail::GetStringView(json);
        const auto isIndexed16 = string.size() == 3 && string.front() == 'i';
        til::color color;

        if (isIndexed16)
        {
            const auto indexStr = string.substr(1);
            const auto idx = til::to_ulong(indexStr, 16);
            color.r = gsl::narrow_cast<uint8_t>(std::min(idx, 15ul));
        }
        else
        {
            color = ::Microsoft::Console::Utils::ColorFromHexString(string);
        }

        MTControl::SelectionColor selection;
        selection.Color(color);
        selection.IsIndex16(isIndexed16);
        return selection;
    }

    bool CanConvert(const Json::Value& json)
    {
        if (!json.isString())
        {
            return false;
        }

        const auto string = Detail::GetStringView(json);
        const auto isColorSpec = (string.length() == 9 || string.length() == 7 || string.length() == 4) && string.front() == '#';
        const auto isIndexedColor = string.size() == 3 && string.front() == 'i';
        return isColorSpec || isIndexedColor;
    }

    Json::Value ToJson(const MTControl::SelectionColor& val)
    {
        const auto color = val.Color();
        if (val.IsIndex16())
        {
            return fmt::format("i{:02x}", color.R);
        }
        else
        {
            return ::Microsoft::Console::Utils::ColorToHexString(color);
        }
    }

    std::string TypeDescription() const
    {
        return "SelectionColor (#rrggbb, #rgb, #rrggbbaa, iNN)";
    }
};
