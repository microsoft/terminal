// Explicit specializations for JSON conversion
JSON_ENUM_MAPPER(CursorStyle)
{
    // Possible values for Cursor Shape
    static constexpr std::string_view CursorShapeVintage{ "vintage" };
    static constexpr std::string_view CursorShapeBar{ "bar" };
    static constexpr std::string_view CursorShapeUnderscore{ "underscore" };
    static constexpr std::string_view CursorShapeFilledbox{ "filledBox" };
    static constexpr std::string_view CursorShapeEmptybox{ "emptyBox" };

    static constexpr std::array<pair_type, 5> mappings = {
        pair_type{ CursorShapeBar, CursorStyle::Bar },
        pair_type{ CursorShapeVintage, CursorStyle::Vintage },
        pair_type{ CursorShapeUnderscore, CursorStyle::Underscore },
        pair_type{ CursorShapeFilledbox, CursorStyle::FilledBox },
        pair_type{ CursorShapeEmptybox, CursorStyle::EmptyBox }
    };
};

JSON_ENUM_MAPPER(Media::Stretch)
{
    // Possible values for Image Stretch Mode
    static constexpr std::string_view ImageStretchModeNone{ "none" };
    static constexpr std::string_view ImageStretchModeFill{ "fill" };
    static constexpr std::string_view ImageStretchModeUniform{ "uniform" };
    static constexpr std::string_view ImageStretchModeUniformToFill{ "uniformToFill" };

    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ ImageStretchModeUniformToFill, Media::Stretch::UniformToFill },
        pair_type{ ImageStretchModeNone, Media::Stretch::None },
        pair_type{ ImageStretchModeFill, Media::Stretch::Fill },
        pair_type{ ImageStretchModeUniform, Media::Stretch::Uniform }
    };
};

JSON_ENUM_MAPPER(ScrollbarState)
{
    // Possible values for Scrollbar state
    static constexpr std::string_view AlwaysVisible{ "visible" };
    static constexpr std::string_view AlwaysHide{ "hidden" };

    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ AlwaysVisible, ScrollbarState::Visible },
        pair_type{ AlwaysHide, ScrollbarState::Hidden }
    };
};

JSON_ENUM_MAPPER(std::tuple<HorizontalAlignment, VerticalAlignment>)
{
    // Possible values for Image Alignment
    static constexpr std::string_view ImageAlignmentCenter{ "center" };
    static constexpr std::string_view ImageAlignmentLeft{ "left" };
    static constexpr std::string_view ImageAlignmentTop{ "top" };
    static constexpr std::string_view ImageAlignmentRight{ "right" };
    static constexpr std::string_view ImageAlignmentBottom{ "bottom" };
    static constexpr std::string_view ImageAlignmentTopLeft{ "topLeft" };
    static constexpr std::string_view ImageAlignmentTopRight{ "topRight" };
    static constexpr std::string_view ImageAlignmentBottomLeft{ "bottomLeft" };
    static constexpr std::string_view ImageAlignmentBottomRight{ "bottomRight" };

    static constexpr std::array<pair_type, 9> mappings = {
        pair_type{ ImageAlignmentCenter, std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Center) },
        pair_type{ ImageAlignmentTopLeft, std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Top) },
        pair_type{ ImageAlignmentBottomLeft, std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Bottom) },
        pair_type{ ImageAlignmentLeft, std::make_tuple(HorizontalAlignment::Left, VerticalAlignment::Center) },
        pair_type{ ImageAlignmentTopRight, std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Top) },
        pair_type{ ImageAlignmentBottomRight, std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Bottom) },
        pair_type{ ImageAlignmentRight, std::make_tuple(HorizontalAlignment::Right, VerticalAlignment::Center) },
        pair_type{ ImageAlignmentTop, std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Top) },
        pair_type{ ImageAlignmentBottom, std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Bottom) }
    };
};

JSON_ENUM_MAPPER(TextAntialiasingMode)
{
    // Possible values for TextAntialiasingMode
    static constexpr std::string_view AntialiasingModeGrayscale{ "grayscale" };
    static constexpr std::string_view AntialiasingModeCleartype{ "cleartype" };
    static constexpr std::string_view AntialiasingModeAliased{ "aliased" };

    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ AntialiasingModeGrayscale, TextAntialiasingMode::Grayscale },
        pair_type{ AntialiasingModeCleartype, TextAntialiasingMode::Cleartype },
        pair_type{ AntialiasingModeAliased, TextAntialiasingMode::Aliased }
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
    // Possible values for closeOnExit
    static constexpr std::string_view CloseOnExitAlways{ "always" };
    static constexpr std::string_view CloseOnExitGraceful{ "graceful" };
    static constexpr std::string_view CloseOnExitNever{ "never" };

    JSON_MAPPINGS(3) = {
        pair_type{ CloseOnExitAlways, CloseOnExitMode::Always },
        pair_type{ CloseOnExitGraceful, CloseOnExitMode::Graceful },
        pair_type{ CloseOnExitNever, CloseOnExitMode::Never },
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

template<>
struct JsonUtils::ConversionTrait<winrt::Windows::UI::Text::FontWeight> : public JsonUtils::EnumMapper<unsigned int, JsonUtils::ConversionTrait<winrt::Windows::UI::Text::FontWeight>>
{
    // Possible values for Font Weight
    static constexpr std::string_view FontWeightThin{ "thin" };
    static constexpr std::string_view FontWeightExtraLight{ "extra-light" };
    static constexpr std::string_view FontWeightLight{ "light" };
    static constexpr std::string_view FontWeightSemiLight{ "semi-light" };
    static constexpr std::string_view FontWeightNormal{ "normal" };
    static constexpr std::string_view FontWeightMedium{ "medium" };
    static constexpr std::string_view FontWeightSemiBold{ "semi-bold" };
    static constexpr std::string_view FontWeightBold{ "bold" };
    static constexpr std::string_view FontWeightExtraBold{ "extra-bold" };
    static constexpr std::string_view FontWeightBlack{ "black" };
    static constexpr std::string_view FontWeightExtraBlack{ "extra-black" };

    // The original parser used the font weight getters Bold(), Normal(), etc.
    // They were both ugly and *not constant expressions*
    JSON_MAPPINGS(11) = {
        pair_type{ FontWeightThin, 100u },
        pair_type{ FontWeightExtraLight, 200u },
        pair_type{ FontWeightLight, 300u },
        pair_type{ FontWeightSemiLight, 350u },
        pair_type{ FontWeightNormal, 400u },
        pair_type{ FontWeightMedium, 500u },
        pair_type{ FontWeightSemiBold, 600u },
        pair_type{ FontWeightBold, 700u },
        pair_type{ FontWeightExtraBold, 800u },
        pair_type{ FontWeightBlack, 900u },
        pair_type{ FontWeightExtraBlack, 950u },
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
            value = EnumMapper::FromJson(json);
        }

        winrt::Windows::UI::Text::FontWeight weight{
            static_cast<uint16_t>(std::clamp(value, 100u, 990u))
        };
        return weight;
    }

    bool CanConvert(const Json::Value& json)
    {
        return EnumMapper::CanConvert(json) || json.isUInt();
    }
};
