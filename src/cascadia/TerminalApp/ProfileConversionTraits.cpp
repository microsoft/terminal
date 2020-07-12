// Explicit specializations for JSON conversion
JSON_ENUM_MAPPER(CursorStyle)
{
    static constexpr std::array<pair_type, 5> mappings = {
        pair_type{ "bar", CursorStyle::Bar },
        pair_type{ "vintage", CursorStyle::Vintage },
        pair_type{ "underscore", CursorStyle::Underscore },
        pair_type{ "filledBox", CursorStyle::FilledBox },
        pair_type{ "emptyBox", CursorStyle::EmptyBox }
    };
};

JSON_ENUM_MAPPER(Media::Stretch)
{
    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ "uniformToFill", Media::Stretch::UniformToFill },
        pair_type{ "none", Media::Stretch::None },
        pair_type{ "fill", Media::Stretch::Fill },
        pair_type{ "uniform", Media::Stretch::Uniform }
    };
};

JSON_ENUM_MAPPER(ScrollbarState)
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ "visible", ScrollbarState::Visible },
        pair_type{ "hidden", ScrollbarState::Hidden }
    };
};

JSON_ENUM_MAPPER(std::tuple<HorizontalAlignment, VerticalAlignment>)
{
    static constexpr std::array<pair_type, 9> mappings = {
        pair_type{ "center", std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Center) },
        pair_type{ "topLeft", std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Top) },
        pair_type{ "bottomLeft", std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Bottom) },
        pair_type{ "left", std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Center) },
        pair_type{ "topRight", std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Top) },
        pair_type{ "bottomRight", std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Bottom) },
        pair_type{ "right", std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Center) },
        pair_type{ "top", std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Top) },
        pair_type{ "bottom", std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Bottom) }
    };
};

JSON_ENUM_MAPPER(TextAntialiasingMode)
{
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ "grayscale", TextAntialiasingMode::Grayscale },
        pair_type{ "cleartype", TextAntialiasingMode::Cleartype },
        pair_type{ "aliased", TextAntialiasingMode::Aliased }
    };
};

// Method Description:
// - Helper function for converting a user-specified closeOnExit value to its corresponding enum
// Arguments:
// - The value from the profiles.json file
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
JSON_ENUM_MAPPER(CloseOnExitMode)
{
    JSON_MAPPINGS(3) = {
        pair_type{ "always", CloseOnExitMode::Always },
        pair_type{ "graceful", CloseOnExitMode::Graceful },
        pair_type{ "never", CloseOnExitMode::Never },
    };

    // Override mapping parser to add boolean parsing
    CloseOnExitMode FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? CloseOnExitMode::Graceful : CloseOnExitMode::Never;
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
struct JsonUtils::ConversionTrait<winrt::Windows::UI::Text::FontWeight> : public JsonUtils::EnumMapper<unsigned int, JsonUtils::ConversionTrait<winrt::Windows::UI::Text::FontWeight>>
{
    // The original parser used the font weight getters Bold(), Normal(), etc.
    // They were both ugly and *not constant expressions*
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

        winrt::Windows::UI::Text::FontWeight weight{
            static_cast<uint16_t>(std::clamp(value, 100u, 990u))
        };
        return weight;
    }

    bool CanConvert(const Json::Value& json)
    {
        return BaseEnumMapper::CanConvert(json) || json.isUInt();
    }
};
