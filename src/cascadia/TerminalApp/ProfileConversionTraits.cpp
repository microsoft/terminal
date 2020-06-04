// Explicit specializations for JSON conversion
JSON_ENUM_MAPPER(CursorStyle)
{
    static constexpr std::array<pair_type, 5> mappings = {
        pair_type{ CursorShapeBar, CursorStyle::Bar }, // DEFAULT
        pair_type{ CursorShapeVintage, CursorStyle::Vintage },
        pair_type{ CursorShapeUnderscore, CursorStyle::Underscore },
        pair_type{ CursorShapeFilledbox, CursorStyle::FilledBox },
        pair_type{ CursorShapeEmptybox, CursorStyle::EmptyBox }
    };
};

JSON_ENUM_MAPPER(Media::Stretch)
{
    static constexpr std::array<pair_type, 4> mappings = {
        pair_type{ ImageStretchModeUniformToFill, Media::Stretch::UniformToFill }, // DEFAULT
        pair_type{ ImageStretchModeNone, Media::Stretch::None },
        pair_type{ ImageStretchModeFill, Media::Stretch::Fill },
        pair_type{ ImageStretchModeUniform, Media::Stretch::Uniform }
    };
};

JSON_ENUM_MAPPER(ScrollbarState)
{
    static constexpr std::array<pair_type, 2> mappings = {
        pair_type{ AlwaysVisible, ScrollbarState::Visible }, // DEFAULT
        pair_type{ AlwaysHide, ScrollbarState::Hidden }
    };
};

JSON_ENUM_MAPPER(std::tuple<HorizontalAlignment, VerticalAlignment>)
{
    static constexpr std::array<pair_type, 9> mappings = {
        pair_type{ ImageAlignmentCenter, std::make_tuple(HorizontalAlignment::Center, VerticalAlignment::Center) }, // DEFAULT
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
    static constexpr std::array<pair_type, 3> mappings = {
        pair_type{ AntialiasingModeGrayscale, TextAntialiasingMode::Grayscale }, // DEFAULT
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
template<>
struct JsonUtils::ConversionTrait<CloseOnExitMode>
{
    CloseOnExitMode FromJson(const Json::Value& json)
    {
        if (json.isBool())
        {
            return json.asBool() ? CloseOnExitMode::Graceful : CloseOnExitMode::Never;
        }

        if (json.isString())
        {
            auto closeOnExit = json.asString();
            if (closeOnExit == CloseOnExitAlways)
            {
                return CloseOnExitMode::Always;
            }
            else if (closeOnExit == CloseOnExitGraceful)
            {
                return CloseOnExitMode::Graceful;
            }
            else if (closeOnExit == CloseOnExitNever)
            {
                return CloseOnExitMode::Never;
            }
        }

        return CloseOnExitMode::Graceful;
    }

    bool CanConvert(const Json::Value& json)
    {
        return json.isString() || json.isBool();
    }
};
