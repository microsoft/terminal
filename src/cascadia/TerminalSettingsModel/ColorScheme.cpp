// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorScheme.h"
#include "../../types/inc/Utils.hpp"
#include "../../types/inc/colorTable.hpp"
#include "Utils.h"
#include "JsonUtils.h"

#include "ColorScheme.g.cpp"

using namespace ::Microsoft::Console;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI;

static constexpr std::string_view NameKey{ "name" };

static constexpr size_t ColorSchemeExpectedSize = 16;
static constexpr std::array<std::pair<std::string_view, size_t>, 18> TableColorsMapping{ {
    // Primary color mappings
    { "black", 0 },
    { "red", 1 },
    { "green", 2 },
    { "yellow", 3 },
    { "blue", 4 },
    { "purple", 5 },
    { "cyan", 6 },
    { "white", 7 },
    { "brightBlack", 8 },
    { "brightRed", 9 },
    { "brightGreen", 10 },
    { "brightYellow", 11 },
    { "brightBlue", 12 },
    { "brightPurple", 13 },
    { "brightCyan", 14 },
    { "brightWhite", 15 },

    // Alternate color mappings (GH#11456)
    { "magenta", 5 },
    { "brightMagenta", 13 },
} };

#define COLORSCHEME_NAMED_COLORS(GEN)                                   \
    GEN(Foreground, "foreground", DEFAULT_FOREGROUND)                   \
    GEN(Background, "background", DEFAULT_BACKGROUND)                   \
    GEN(SelectionBackground, "selectionBackground", DEFAULT_FOREGROUND) \
    GEN(CursorColor, "cursorColor", DEFAULT_CURSOR_COLOR)

// The named colors paired with their default values. Used to drive the
// constructor seed, _layerJson validation, and ToJson copy from a single list.
// The individual getters still reference their DEFAULT_* constant directly.
#define GEN_NAMED_COLOR_DEFAULT(name, jsonKey, defaultVal) \
    { jsonKey, static_cast<winrt::Microsoft::Terminal::Core::Color>(defaultVal) },
static const std::array<std::pair<std::string_view, winrt::Microsoft::Terminal::Core::Color>, 4> NamedColorDefaults{ {
    COLORSCHEME_NAMED_COLORS(GEN_NAMED_COLOR_DEFAULT)
} };
#undef GEN_NAMED_COLOR_DEFAULT

ColorScheme::ColorScheme() noexcept :
    ColorScheme{ winrt::hstring{} }
{
}

ColorScheme::ColorScheme(const winrt::hstring& name) noexcept :
    _Origin{ OriginTag::User }
{
    JsonUtils::SetValueForKey(_json, NameKey, name);
    for (const auto& [key, defaultValue] : NamedColorDefaults)
    {
        JsonUtils::SetValueForKey(_json, key, defaultValue);
    }

    const auto table = Utils::CampbellColorTable();
    for (size_t i = 0; i < ColorSchemeExpectedSize; ++i)
    {
        const Core::Color color = til::at(table, i);
        JsonUtils::SetValueForKey(_json, til::at(TableColorsMapping, i).first, color);
    }
}

winrt::com_ptr<ColorScheme> ColorScheme::Copy() const
{
    auto scheme{ winrt::make_self<ColorScheme>(uninitialized_t{}) };
    scheme->_json = _json;
    scheme->_Origin = _Origin;
    return scheme;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - Returns nullptr for invalid JSON.
winrt::com_ptr<ColorScheme> ColorScheme::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<ColorScheme>(uninitialized_t{});
    return result->_layerJson(json) ? result : nullptr;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// Arguments:
// - json: an object which should be a full serialization of a ColorScheme object.
// Return Value:
// - Returns true if the given JSON was valid.
bool ColorScheme::_layerJson(const Json::Value& json)
{
    // Required fields
    winrt::hstring name{};
    auto isValid = JsonUtils::GetValueForKey(json, NameKey, name);

    // Optional fields; parsing them here only validates their type.
    Core::Color namedColor{};
    for (const auto& [key, _] : NamedColorDefaults)
    {
        JsonUtils::GetValueForKey(json, key, namedColor);
    }

    // Required fields
    std::array<Core::Color, COLOR_TABLE_SIZE> table{};
    size_t colorCount = 0;
    for (const auto& [key, index] : TableColorsMapping)
    {
        colorCount += JsonUtils::GetValueForKey(json, key, til::at(table, index));
        if (colorCount == ColorSchemeExpectedSize)
        {
            break;
        }
    }

    isValid &= (colorCount == 16); // Valid schemes should have exactly 16 colors
    if (!isValid)
    {
        return false;
    }

    JsonUtils::MergeJsonKeys(json, _json);
    _normalizeTableAliasKeys();
    return true;
}

// Moves any alias color key (magenta -> purple, brightMagenta -> brightPurple;
// GH#11456) onto its canonical key so getters/ToJson only deal with canonical
// keys. The canonical key wins if both are present (matching the _layerJson
// validation loop's canonical-first preference). The node is copied whole, so an
// attached comment travels with the value.
void ColorScheme::_normalizeTableAliasKeys()
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 2> aliases{ {
        { "magenta", "purple" },
        { "brightMagenta", "brightPurple" },
    } };

    for (const auto& [alias, canonical] : aliases)
    {
        const auto aliasKey = JsonKey(alias);
        if (_json.isMember(aliasKey))
        {
            const auto canonicalKey = JsonKey(canonical);
            if (!_json.isMember(canonicalKey))
            {
                // Copy via a temporary to avoid jsoncpp self-aliasing concerns.
                const Json::Value moved = _json[aliasKey];
                _json[canonicalKey] = moved;
            }
            _json.removeMember(aliasKey);
        }
    }
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value ColorScheme::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::CopyKeyIfPresent(_json, json, NameKey);
    for (const auto& [key, _] : NamedColorDefaults)
    {
        JsonUtils::CopyKeyIfPresent(_json, json, key);
    }

    for (uint32_t i = 0; i < ColorSchemeExpectedSize; ++i)
    {
        JsonUtils::CopyKeyIfPresent(_json, json, til::at(TableColorsMapping, i).first);
    }

    return json;
}

winrt::hstring ColorScheme::Name() const
{
    winrt::hstring name{};
    JsonUtils::GetValueForKey(_json, NameKey, name);
    return name;
}

void ColorScheme::Name(const winrt::hstring& value)
{
    JsonUtils::SetValueForKeyPreservingComments(_json, NameKey, value);
}

// Reads a color from _json, falling back to defaultValue when the key is absent.
winrt::Microsoft::Terminal::Core::Color ColorScheme::_getColor(std::string_view key, const Core::Color& defaultValue) const
{
    auto value = defaultValue;
    JsonUtils::GetValueForKey(_json, key, value);
    return value;
}

// Writes a color to _json while preserving any comment attached to the key.
void ColorScheme::_setColor(std::string_view key, const Core::Color& value)
{
    JsonUtils::SetValueForKeyPreservingComments(_json, key, value);
}

#define GEN_NAMED_COLOR_ACCESSOR(name, jsonKey, defaultVal)             \
    winrt::Microsoft::Terminal::Core::Color ColorScheme::name() const   \
    {                                                                   \
        return _getColor(jsonKey, static_cast<Core::Color>(defaultVal)); \
    }                                                                   \
    void ColorScheme::name(const Core::Color& value)                    \
    {                                                                   \
        _setColor(jsonKey, value);                                      \
    }
COLORSCHEME_NAMED_COLORS(GEN_NAMED_COLOR_ACCESSOR)
#undef GEN_NAMED_COLOR_ACCESSOR

#undef COLORSCHEME_NAMED_COLORS

// Method Description:
// - Returns the resolved 16-entry color table, reading each entry from _json and
//   falling back to the Campbell value for any entry not set.
winrt::com_array<winrt::Microsoft::Terminal::Core::Color> ColorScheme::Table() const
{
    std::array<Core::Color, COLOR_TABLE_SIZE> table{};
    const auto campbell = Utils::CampbellColorTable();
    for (size_t i = 0; i < ColorSchemeExpectedSize; ++i)
    {
        til::at(table, i) = _getColor(til::at(TableColorsMapping, i).first, static_cast<Core::Color>(til::at(campbell, i)));
    }
    return winrt::com_array<Core::Color>{ table };
}

// Method Description:
// - Set a color in the color table
// Arguments:
// - index: the index of the desired color within the table
// - value: the color value we are setting the color table color to
// Return Value:
// - none
void ColorScheme::SetColorTableEntry(uint8_t index, const Core::Color& value)
{
    THROW_HR_IF(E_INVALIDARG, index >= COLOR_TABLE_SIZE);
    _setColor(til::at(TableColorsMapping, index).first, value);
}

bool ColorScheme::IsEquivalentForSettingsMergePurposes(const winrt::com_ptr<ColorScheme>& other) noexcept
{
    // The caller likely only got here if the names were the same, so skip checking that one.
    // We do not care about the cursor color or the selection background, as the main reason we are
    // doing equivalence merging is to replace old, poorly-specified versions of those two properties.
    const auto thisTable = Table();
    const auto otherTable = other->Table();
    for (uint32_t i = 0; i < thisTable.size(); ++i)
    {
        if (!(thisTable[i] == otherTable[i]))
        {
            return false;
        }
    }
    return Background() == other->Background() && Foreground() == other->Foreground();
}
