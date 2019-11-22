// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

#include "LegacyProfileGeneratorNamespaces.h"

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Console;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view GuidKey{ "guid" };
static constexpr std::string_view SourceKey{ "source" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };
static constexpr std::string_view ColorSchemeKeyOld{ "colorscheme" };
static constexpr std::string_view HiddenKey{ "hidden" };

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view ColorTableKey{ "colorTable" };
static constexpr std::string_view TabTitleKey{ "tabTitle" };
static constexpr std::string_view SuppressApplicationTitleKey{ "suppressApplicationTitle" };
static constexpr std::string_view HistorySizeKey{ "historySize" };
static constexpr std::string_view SnapOnInputKey{ "snapOnInput" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view CursorShapeKey{ "cursorShape" };
static constexpr std::string_view CursorHeightKey{ "cursorHeight" };

static constexpr std::string_view ConnectionTypeKey{ "connectionType" };
static constexpr std::string_view CommandlineKey{ "commandline" };
static constexpr std::string_view FontFaceKey{ "fontFace" };
static constexpr std::string_view FontSizeKey{ "fontSize" };
static constexpr std::string_view AcrylicTransparencyKey{ "acrylicOpacity" };
static constexpr std::string_view UseAcrylicKey{ "useAcrylic" };
static constexpr std::string_view ScrollbarStateKey{ "scrollbarState" };
static constexpr std::string_view CloseOnExitKey{ "closeOnExit" };
static constexpr std::string_view PaddingKey{ "padding" };
static constexpr std::string_view StartingDirectoryKey{ "startingDirectory" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view BackgroundImageKey{ "backgroundImage" };
static constexpr std::string_view BackgroundImageOpacityKey{ "backgroundImageOpacity" };
static constexpr std::string_view BackgroundImageStretchModeKey{ "backgroundImageStretchMode" };
static constexpr std::string_view BackgroundImageAlignmentKey{ "backgroundImageAlignment" };

// Possible values for Scrollbar state
static constexpr std::wstring_view AlwaysVisible{ L"visible" };
static constexpr std::wstring_view AlwaysHide{ L"hidden" };

// Possible values for Cursor Shape
static constexpr std::wstring_view CursorShapeVintage{ L"vintage" };
static constexpr std::wstring_view CursorShapeBar{ L"bar" };
static constexpr std::wstring_view CursorShapeUnderscore{ L"underscore" };
static constexpr std::wstring_view CursorShapeFilledbox{ L"filledBox" };
static constexpr std::wstring_view CursorShapeEmptybox{ L"emptyBox" };

// Possible values for Image Stretch Mode
static constexpr std::string_view ImageStretchModeNone{ "none" };
static constexpr std::string_view ImageStretchModeFill{ "fill" };
static constexpr std::string_view ImageStretchModeUniform{ "uniform" };
static constexpr std::string_view ImageStretchModeUniformTofill{ "uniformToFill" };

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

Profile::Profile() :
    Profile(std::nullopt)
{
}

Profile::Profile(const std::optional<GUID>& guid) :
    _guid(guid),
    _name{ L"Default" },
    _schemeName{ L"Campbell" },
    _hidden{ false },

    _defaultForeground{},
    _defaultBackground{},
    _selectionBackground{},
    _colorTable{},
    _tabTitle{},
    _suppressApplicationTitle{},
    _historySize{ DEFAULT_HISTORY_SIZE },
    _snapOnInput{ true },
    _cursorColor{ DEFAULT_CURSOR_COLOR },
    _cursorShape{ CursorStyle::Bar },
    _cursorHeight{ DEFAULT_CURSOR_HEIGHT },

    _connectionType{},
    _commandline{ L"cmd.exe" },
    _startingDirectory{},
    _fontFace{ DEFAULT_FONT_FACE },
    _fontSize{ DEFAULT_FONT_SIZE },
    _acrylicTransparency{ 0.5 },
    _useAcrylic{ false },
    _scrollbarState{},
    _closeOnExit{ true },
    _padding{ DEFAULT_PADDING },
    _icon{},
    _backgroundImage{},
    _backgroundImageOpacity{},
    _backgroundImageStretchMode{},
    _backgroundImageAlignment{}
{
}

Profile::~Profile()
{
}

bool Profile::HasGuid() const noexcept
{
    return _guid.has_value();
}

bool Profile::HasSource() const noexcept
{
    return _source.has_value();
}

GUID Profile::GetGuid() const noexcept
{
    // This can throw if we never had our guid set to a legitimate value.
    THROW_HR_IF_MSG(E_FAIL, !_guid.has_value(), "Profile._guid always expected to have a value");
    return _guid.value();
}

void Profile::SetSource(std::wstring_view sourceNamespace) noexcept
{
    _source = sourceNamespace;
}

// Method Description:
// - Create a TerminalSettings from this object. Apply our settings, as well as
//      any colors from our color scheme, if we have one.
// Arguments:
// - schemes: a list of schemes to look for our color scheme in, if we have one.
// Return Value:
// - a new TerminalSettings object with our settings in it.
TerminalSettings Profile::CreateTerminalSettings(const std::unordered_map<std::wstring, ColorScheme>& schemes) const
{
    TerminalSettings terminalSettings{};

    // Fill in the Terminal Setting's CoreSettings from the profile
    auto const colorTableCount = gsl::narrow_cast<int>(_colorTable.size());
    for (int i = 0; i < colorTableCount; i++)
    {
        terminalSettings.SetColorTableEntry(i, _colorTable[i]);
    }
    terminalSettings.HistorySize(_historySize);
    terminalSettings.SnapOnInput(_snapOnInput);
    terminalSettings.CursorColor(_cursorColor);
    terminalSettings.CursorHeight(_cursorHeight);
    terminalSettings.CursorShape(_cursorShape);

    // Fill in the remaining properties from the profile
    terminalSettings.UseAcrylic(_useAcrylic);
    terminalSettings.CloseOnExit(_closeOnExit);
    terminalSettings.TintOpacity(_acrylicTransparency);

    terminalSettings.FontFace(_fontFace);
    terminalSettings.FontSize(_fontSize);
    terminalSettings.Padding(_padding);

    terminalSettings.Commandline(winrt::to_hstring(_commandline.c_str()));

    if (_startingDirectory)
    {
        const auto evaluatedDirectory = Profile::EvaluateStartingDirectory(_startingDirectory.value());
        terminalSettings.StartingDirectory(winrt::to_hstring(evaluatedDirectory.c_str()));
    }

    // GH#2373: Use the tabTitle as the starting title if it exists, otherwise
    // use the profile name
    terminalSettings.StartingTitle(_tabTitle ? _tabTitle.value() : _name);

    if (_suppressApplicationTitle)
    {
        terminalSettings.SuppressApplicationTitle(_suppressApplicationTitle);
    }

    if (_schemeName)
    {
        const auto found = schemes.find(_schemeName.value());
        if (found != schemes.end())
        {
            found->second.ApplyScheme(terminalSettings);
        }
    }
    if (_defaultForeground)
    {
        terminalSettings.DefaultForeground(_defaultForeground.value());
    }
    if (_defaultBackground)
    {
        terminalSettings.DefaultBackground(_defaultBackground.value());
    }
    if (_selectionBackground)
    {
        terminalSettings.SelectionBackground(_selectionBackground.value());
    }

    if (_scrollbarState)
    {
        ScrollbarState result = ParseScrollbarState(_scrollbarState.value());
        terminalSettings.ScrollState(result);
    }

    if (HasBackgroundImage())
    {
        terminalSettings.BackgroundImage(GetExpandedBackgroundImagePath().c_str());
    }

    if (_backgroundImageOpacity)
    {
        terminalSettings.BackgroundImageOpacity(_backgroundImageOpacity.value());
    }

    if (_backgroundImageStretchMode)
    {
        terminalSettings.BackgroundImageStretchMode(_backgroundImageStretchMode.value());
    }

    if (_backgroundImageAlignment)
    {
        const auto imageHorizontalAlignment = std::get<HorizontalAlignment>(_backgroundImageAlignment.value());
        const auto imageVerticalAlignment = std::get<VerticalAlignment>(_backgroundImageAlignment.value());
        terminalSettings.BackgroundImageHorizontalAlignment(imageHorizontalAlignment);
        terminalSettings.BackgroundImageVerticalAlignment(imageVerticalAlignment);
    }

    return terminalSettings;
}

// Method Description:
// - Serialize this object to a JsonObject.
// Arguments:
// - <none>
// Return Value:
// - a JsonObject which is an equivalent serialization of this object.
Json::Value Profile::ToJson() const
{
    Json::Value root = GenerateStub();

    ///// Profile-specific settings /////
    // As of #2795, all profile-specific settings were moved to GenerateStub. If
    // any new profiles-specific settings are added, they should probably be
    // added here instead of in that method.

    ///// Core Settings /////
    if (_defaultForeground)
    {
        root[JsonKey(ForegroundKey)] = Utils::ColorToHexString(_defaultForeground.value());
    }
    if (_defaultBackground)
    {
        root[JsonKey(BackgroundKey)] = Utils::ColorToHexString(_defaultBackground.value());
    }
    if (_selectionBackground)
    {
        root[JsonKey(SelectionBackgroundKey)] = Utils::ColorToHexString(_selectionBackground.value());
    }
    if (_schemeName)
    {
        const auto scheme = winrt::to_string(_schemeName.value());
        root[JsonKey(ColorSchemeKey)] = scheme;
    }
    else
    {
        Json::Value tableArray{};
        for (auto& color : _colorTable)
        {
            tableArray.append(Utils::ColorToHexString(color));
        }
        root[JsonKey(ColorTableKey)] = tableArray;
    }
    root[JsonKey(HistorySizeKey)] = _historySize;
    root[JsonKey(SnapOnInputKey)] = _snapOnInput;
    root[JsonKey(CursorColorKey)] = Utils::ColorToHexString(_cursorColor);
    // Only add the cursor height property if we're a legacy-style cursor.
    if (_cursorShape == CursorStyle::Vintage)
    {
        root[JsonKey(CursorHeightKey)] = _cursorHeight;
    }
    root[JsonKey(CursorShapeKey)] = winrt::to_string(_SerializeCursorStyle(_cursorShape));

    ///// Control Settings /////
    root[JsonKey(CommandlineKey)] = winrt::to_string(_commandline);
    root[JsonKey(FontFaceKey)] = winrt::to_string(_fontFace);
    root[JsonKey(FontSizeKey)] = _fontSize;
    root[JsonKey(AcrylicTransparencyKey)] = _acrylicTransparency;
    root[JsonKey(UseAcrylicKey)] = _useAcrylic;
    root[JsonKey(CloseOnExitKey)] = _closeOnExit;
    root[JsonKey(PaddingKey)] = winrt::to_string(_padding);

    if (_connectionType)
    {
        root[JsonKey(ConnectionTypeKey)] = winrt::to_string(Utils::GuidToString(_connectionType.value()));
    }
    if (_scrollbarState)
    {
        const auto scrollbarState = winrt::to_string(_scrollbarState.value());
        root[JsonKey(ScrollbarStateKey)] = scrollbarState;
    }

    if (_icon)
    {
        const auto icon = winrt::to_string(_icon.value());
        root[JsonKey(IconKey)] = icon;
    }

    if (_tabTitle)
    {
        root[JsonKey(TabTitleKey)] = winrt::to_string(_tabTitle.value());
    }

    if (_suppressApplicationTitle)
    {
        root[JsonKey(SuppressApplicationTitleKey)] = _suppressApplicationTitle;
    }

    if (_startingDirectory)
    {
        root[JsonKey(StartingDirectoryKey)] = winrt::to_string(_startingDirectory.value());
    }

    if (_backgroundImage)
    {
        root[JsonKey(BackgroundImageKey)] = winrt::to_string(_backgroundImage.value());
    }

    if (_backgroundImageOpacity)
    {
        root[JsonKey(BackgroundImageOpacityKey)] = _backgroundImageOpacity.value();
    }

    if (_backgroundImageStretchMode)
    {
        root[JsonKey(BackgroundImageStretchModeKey)] = SerializeImageStretchMode(_backgroundImageStretchMode.value()).data();
    }

    if (_backgroundImageAlignment)
    {
        root[JsonKey(BackgroundImageAlignmentKey)] = SerializeImageAlignment(_backgroundImageAlignment.value()).data();
    }

    return root;
}

// Method Description:
// - This generates a json object `diff` s.t.
//      this = other.LayerJson(diff)
// So if:
// - this has a nullopt for an optional, diff will have null for that member
// - this has a value for an optional, diff will have our value. If the other
//   did _not_ have a value, and we did, diff will have our value.
// Arguments:
// - other: the other profile object to use as the "base" for this diff. The
//   result could be layered upon that json object to re-create this object's
//   serialization.
// Return Value:
// - a diff between this and the other object, such that this could be recreated
//   from the diff and the other object.
Json::Value Profile::DiffToJson(const Profile& other) const
{
    auto otherJson = other.ToJson();
    auto myJson = ToJson();
    Json::Value diff;

    // Iterate in two steps:
    // - first over all the keys in the 'other' object's serialization.
    // - then over all the keys in our serialization.
    // In this way, we ensure all keys from both objects are present in the
    // final object.
    for (const auto& key : otherJson.getMemberNames())
    {
        if (myJson.isMember(key))
        {
            // Both objects have the key
            auto otherVal = otherJson[key];
            auto myVal = myJson[key];
            if (otherVal != myVal)
            {
                diff[key] = myVal;
            }
        }
        else
        {
            // key is not in this json object. Set to null, so that when the
            // diff is layered upon the original object, we'll properly set
            // nullopt for any optionals that weren't present in this object.
            diff[key] = Json::Value::null;
        }
    }
    for (const auto& key : myJson.getMemberNames())
    {
        if (otherJson.isMember(key))
        {
            // both objects have this key. Do nothing, this is handled above
        }
        else
        {
            // We have a key the other object did not. Add our value.
            diff[key] = myJson[key];
        }
    }

    return diff;
}

// Method Description:
// - Generates a Json::Value which is a "stub" of this profile. This stub will
//   have enough information that it could be layered with this profile.
// - This method is used during dynamic profile generation - if a profile is
//   ever generated that didn't already exist in the user's settings, we'll add
//   this stub to the user's settings file, so the user has an easy point to
//   modify the generated profile.
// Arguments:
// - <none>
// Return Value:
// - A json::Value with a guid, name and source (if applicable).
Json::Value Profile::GenerateStub() const
{
    Json::Value stub;

    ///// Profile-specific settings /////
    if (_guid.has_value())
    {
        stub[JsonKey(GuidKey)] = winrt::to_string(Utils::GuidToString(_guid.value()));
    }

    stub[JsonKey(NameKey)] = winrt::to_string(_name);

    if (_source.has_value())
    {
        stub[JsonKey(SourceKey)] = winrt::to_string(_source.value());
    }

    stub[JsonKey(HiddenKey)] = _hidden;

    return stub;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
Profile Profile::FromJson(const Json::Value& json)
{
    Profile result;

    result.LayerJson(json);

    return result;
}

// Method Description:
// - Returns true if we think the provided json object represents an instance of
//   the same object as this object. If true, we should layer that json object
//   on us, instead of creating a new object.
// Arguments:
// - json: The json object to query to see if it's the same
// Return Value:
// - true iff the json object has the same `GUID` as we do.
bool Profile::ShouldBeLayered(const Json::Value& json) const
{
    if (!_guid.has_value())
    {
        return false;
    }

    // First, check that GUIDs match. This is easy. If they don't match, they
    // should _definitely_ not layer.
    if (json.isMember(JsonKey(GuidKey)))
    {
        const auto guid{ json[JsonKey(GuidKey)] };
        const auto otherGuid = Utils::GuidFromString(GetWstringFromJson(guid));
        if (_guid.value() != otherGuid)
        {
            return false;
        }
    }
    else
    {
        // If the other json object didn't have a GUID, we definitely don't want
        // to layer. We technically might have the same name, and would
        // auto-generate the same guid, but they should be treated as different
        // profiles.
        return false;
    }

    const auto& otherSource = json.isMember(JsonKey(SourceKey)) ? json[JsonKey(SourceKey)] : Json::Value::null;

    // For profiles with a `source`, also check the `source` property.
    bool sourceMatches = false;
    if (_source.has_value())
    {
        if (json.isMember(JsonKey(SourceKey)))
        {
            const auto otherSourceString = GetWstringFromJson(otherSource);
            sourceMatches = otherSourceString == _source.value();
        }
        else
        {
            // Special case the legacy dynamic profiles here. In this case,
            // `this` is a dynamic profile with a source, and our _source is one
            // of the legacy DPG namespaces. We're looking to see if the other
            // json object has the same guid, but _no_ "source"
            if (_source.value() == WslGeneratorNamespace ||
                _source.value() == AzureGeneratorNamespace ||
                _source.value() == PowershellCoreGeneratorNamespace)
            {
                sourceMatches = true;
            }
        }
    }
    else
    {
        // We do not have a source. The only way we match is if source is set to null or "".
        if (otherSource.isNull() || (otherSource.isString() && otherSource == ""))
        {
            sourceMatches = true;
        }
    }

    return sourceMatches;
}

// Method Description:
// - Helper function to convert a json value into a value of the Stretch enum.
//   Calls into ParseImageStretchMode. Used with JsonUtils::GetOptionalValue.
// Arguments:
// - json: the Json::Value object to parse.
// Return Value:
// - An appropriate value from Windows.UI.Xaml.Media.Stretch
Media::Stretch Profile::_ConvertJsonToStretchMode(const Json::Value& json)
{
    return Profile::ParseImageStretchMode(json.asString());
}

// Method Description:
// - Helper function to convert a json value into a value of the Stretch enum.
//   Calls into ParseImageAlignment. Used with JsonUtils::GetOptionalValue.
// Arguments:
// - json: the Json::Value object to parse.
// Return Value:
// - A pair of HorizontalAlignment and VerticalAlignment
std::tuple<HorizontalAlignment, VerticalAlignment> Profile::_ConvertJsonToAlignment(const Json::Value& json)
{
    return Profile::ParseImageAlignment(json.asString());
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// - Optional values in the profile that are set to `null` in the json object
//   will be set to nullopt.
// Arguments:
// - json: an object which should be a partial serialization of a Profile object.
// Return Value:
// <none>
void Profile::LayerJson(const Json::Value& json)
{
    // Profile-specific Settings
    if (json.isMember(JsonKey(NameKey)))
    {
        auto name{ json[JsonKey(NameKey)] };
        _name = GetWstringFromJson(name);
    }

    JsonUtils::GetOptionalGuid(json, GuidKey, _guid);

    if (json.isMember(JsonKey(HiddenKey)))
    {
        auto hidden{ json[JsonKey(HiddenKey)] };
        _hidden = hidden.asBool();
    }

    // Core Settings
    JsonUtils::GetOptionalColor(json, ForegroundKey, _defaultForeground);

    JsonUtils::GetOptionalColor(json, BackgroundKey, _defaultBackground);

    JsonUtils::GetOptionalColor(json, SelectionBackgroundKey, _selectionBackground);

    JsonUtils::GetOptionalString(json, ColorSchemeKey, _schemeName);
    // TODO:GH#1069 deprecate old settings key
    JsonUtils::GetOptionalString(json, ColorSchemeKeyOld, _schemeName);

    // Only look for the "table" if there's no "schemeName"
    if (!(json.isMember(JsonKey(ColorSchemeKey))) &&
        !(json.isMember(JsonKey(ColorSchemeKeyOld))) &&
        json.isMember(JsonKey(ColorTableKey)))
    {
        auto colortable{ json[JsonKey(ColorTableKey)] };
        int i = 0;
        for (const auto& tableEntry : colortable)
        {
            if (tableEntry.isString())
            {
                const auto color = Utils::ColorFromHexString(tableEntry.asString());
                _colorTable[i] = color;
            }
            i++;
        }
    }
    if (json.isMember(JsonKey(HistorySizeKey)))
    {
        auto historySize{ json[JsonKey(HistorySizeKey)] };
        // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
        _historySize = historySize.asInt();
    }
    if (json.isMember(JsonKey(SnapOnInputKey)))
    {
        auto snapOnInput{ json[JsonKey(SnapOnInputKey)] };
        _snapOnInput = snapOnInput.asBool();
    }
    if (json.isMember(JsonKey(CursorColorKey)))
    {
        auto cursorColor{ json[JsonKey(CursorColorKey)] };
        const auto color = Utils::ColorFromHexString(cursorColor.asString());
        _cursorColor = color;
    }
    if (json.isMember(JsonKey(CursorHeightKey)))
    {
        auto cursorHeight{ json[JsonKey(CursorHeightKey)] };
        _cursorHeight = cursorHeight.asUInt();
    }
    if (json.isMember(JsonKey(CursorShapeKey)))
    {
        auto cursorShape{ json[JsonKey(CursorShapeKey)] };
        _cursorShape = _ParseCursorShape(GetWstringFromJson(cursorShape));
    }
    JsonUtils::GetOptionalString(json, TabTitleKey, _tabTitle);

    // Control Settings
    JsonUtils::GetOptionalGuid(json, ConnectionTypeKey, _connectionType);

    if (json.isMember(JsonKey(CommandlineKey)))
    {
        auto commandline{ json[JsonKey(CommandlineKey)] };
        _commandline = GetWstringFromJson(commandline);
    }
    if (json.isMember(JsonKey(FontFaceKey)))
    {
        auto fontFace{ json[JsonKey(FontFaceKey)] };
        _fontFace = GetWstringFromJson(fontFace);
    }
    if (json.isMember(JsonKey(FontSizeKey)))
    {
        auto fontSize{ json[JsonKey(FontSizeKey)] };
        _fontSize = fontSize.asInt();
    }
    if (json.isMember(JsonKey(AcrylicTransparencyKey)))
    {
        auto acrylicTransparency{ json[JsonKey(AcrylicTransparencyKey)] };
        _acrylicTransparency = acrylicTransparency.asFloat();
    }
    if (json.isMember(JsonKey(UseAcrylicKey)))
    {
        auto useAcrylic{ json[JsonKey(UseAcrylicKey)] };
        _useAcrylic = useAcrylic.asBool();
    }
    if (json.isMember(JsonKey(SuppressApplicationTitleKey)))
    {
        auto suppressApplicationTitle{ json[JsonKey(SuppressApplicationTitleKey)] };
        _suppressApplicationTitle = suppressApplicationTitle.asBool();
    }
    if (json.isMember(JsonKey(CloseOnExitKey)))
    {
        auto closeOnExit{ json[JsonKey(CloseOnExitKey)] };
        _closeOnExit = closeOnExit.asBool();
    }
    if (json.isMember(JsonKey(PaddingKey)))
    {
        auto padding{ json[JsonKey(PaddingKey)] };
        _padding = GetWstringFromJson(padding);
    }

    JsonUtils::GetOptionalString(json, ScrollbarStateKey, _scrollbarState);

    JsonUtils::GetOptionalString(json, StartingDirectoryKey, _startingDirectory);

    JsonUtils::GetOptionalString(json, IconKey, _icon);

    JsonUtils::GetOptionalString(json, BackgroundImageKey, _backgroundImage);

    JsonUtils::GetOptionalDouble(json, BackgroundImageOpacityKey, _backgroundImageOpacity);

    JsonUtils::GetOptionalValue(json, BackgroundImageStretchModeKey, _backgroundImageStretchMode, &Profile::_ConvertJsonToStretchMode);

    JsonUtils::GetOptionalValue(json, BackgroundImageAlignmentKey, _backgroundImageAlignment, &Profile::_ConvertJsonToAlignment);
}

void Profile::SetFontFace(std::wstring fontFace) noexcept
{
    _fontFace = std::move(fontFace);
}

void Profile::SetColorScheme(std::optional<std::wstring> schemeName) noexcept
{
    _schemeName = std::move(schemeName);
}

std::optional<std::wstring>& Profile::GetSchemeName() noexcept
{
    return _schemeName;
}

void Profile::SetAcrylicOpacity(double opacity) noexcept
{
    _acrylicTransparency = opacity;
}

void Profile::SetCommandline(std::wstring cmdline) noexcept
{
    _commandline = std::move(cmdline);
}

void Profile::SetStartingDirectory(std::wstring startingDirectory) noexcept
{
    _startingDirectory = std::move(startingDirectory);
}

void Profile::SetName(std::wstring name) noexcept
{
    _name = std::move(name);
}

void Profile::SetUseAcrylic(bool useAcrylic) noexcept
{
    _useAcrylic = useAcrylic;
}

void Profile::SetDefaultForeground(COLORREF defaultForeground) noexcept
{
    _defaultForeground = defaultForeground;
}

void Profile::SetDefaultBackground(COLORREF defaultBackground) noexcept
{
    _defaultBackground = defaultBackground;
}

void Profile::SetSelectionBackground(COLORREF selectionBackground) noexcept
{
    _selectionBackground = selectionBackground;
}

void Profile::SetCloseOnExit(bool defaultClose) noexcept
{
    _closeOnExit = defaultClose;
}

void Profile::SetConnectionType(GUID connectionType) noexcept
{
    _connectionType = connectionType;
}

bool Profile::HasIcon() const noexcept
{
    return _icon.has_value() && !_icon.value().empty();
}

bool Profile::HasBackgroundImage() const noexcept
{
    return _backgroundImage.has_value() && !_backgroundImage.value().empty();
}

// Method Description
// - Sets this profile's tab title.
// Arguments:
// - tabTitle: the tab title
void Profile::SetTabTitle(std::wstring tabTitle) noexcept
{
    _tabTitle = std::move(tabTitle);
}

// Method Description
// - Sets if the application title will be suppressed in this profile.
// Arguments:
// - suppressApplicationTitle: boolean
void Profile::SetSuppressApplicationTitle(bool suppressApplicationTitle) noexcept
{
    _suppressApplicationTitle = suppressApplicationTitle;
}

// Method Description:
// - Sets this profile's icon path.
// Arguments:
// - path: the path
void Profile::SetIconPath(std::wstring_view path)
{
    static_assert(!noexcept(_icon.emplace(path)));
    _icon.emplace(path);
}

// Method Description:
// - Returns this profile's icon path, if one is set. Otherwise returns the
//   empty string. This method will expand any environment variables in the
//   path, if there are any.
// Return Value:
// - this profile's icon path, if one is set. Otherwise returns the empty string.
winrt::hstring Profile::GetExpandedIconPath() const
{
    if (!HasIcon())
    {
        return { L"" };
    }
    winrt::hstring envExpandedPath{ wil::ExpandEnvironmentStringsW<std::wstring>(_icon.value().data()) };
    return envExpandedPath;
}

// Method Description:
// - Returns this profile's background image path, if one is set, expanding
//   any environment variables in the path, if there are any.
// Return Value:
// - This profile's expanded background image path / the empty string.
winrt::hstring Profile::GetExpandedBackgroundImagePath() const
{
    winrt::hstring result{};

    if (HasBackgroundImage())
    {
        result = wil::ExpandEnvironmentStringsW<std::wstring>(_backgroundImage.value().data());
    }

    return result;
}

// Method Description:
// - Returns the name of this profile.
// Arguments:
// - <none>
// Return Value:
// - the name of this profile
std::wstring_view Profile::GetName() const noexcept
{
    return _name;
}

bool Profile::GetSuppressApplicationTitle() const noexcept
{
    return _suppressApplicationTitle;
}

bool Profile::HasConnectionType() const noexcept
{
    return _connectionType.has_value();
}

GUID Profile::GetConnectionType() const noexcept
{
    return HasConnectionType() ?
               _connectionType.value() :
               _GUID{};
}

bool Profile::GetCloseOnExit() const noexcept
{
    return _closeOnExit;
}

// Method Description:
// - If a profile is marked hidden, it should not appear in the dropdown list of
//   profiles. This setting is used to "remove" default and dynamic profiles
//   from the list of profiles.
// Arguments:
// - <none>
// Return Value:
// - true iff the profile chould be hidden from the list of profiles.
bool Profile::IsHidden() const noexcept
{
    return _hidden;
}

// Method Description:
// - Helper function for expanding any environment variables in a user-supplied starting directory and validating the resulting path
// Arguments:
// - The value from the profiles.json file
// Return Value:
// - The directory string with any environment variables expanded. If the resulting path is invalid,
// - the function returns an evaluated version of %userprofile% to avoid blocking the session from starting.
std::wstring Profile::EvaluateStartingDirectory(const std::wstring& directory)
{
    // First expand path
    DWORD numCharsInput = ExpandEnvironmentStrings(directory.c_str(), nullptr, 0);
    std::unique_ptr<wchar_t[]> evaluatedPath = std::make_unique<wchar_t[]>(numCharsInput);
    THROW_LAST_ERROR_IF(0 == ExpandEnvironmentStrings(directory.c_str(), evaluatedPath.get(), numCharsInput));

    // Validate that the resulting path is legitimate
    const DWORD dwFileAttributes = GetFileAttributes(evaluatedPath.get());
    if ((dwFileAttributes != INVALID_FILE_ATTRIBUTES) && (WI_IsFlagSet(dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY)))
    {
        return std::wstring(evaluatedPath.get(), numCharsInput);
    }
    else
    {
        // In the event where the user supplied a path that can't be resolved, use a reasonable default (in this case, %userprofile%)
        const DWORD numCharsDefault = ExpandEnvironmentStrings(DEFAULT_STARTING_DIRECTORY.c_str(), nullptr, 0);
        std::unique_ptr<wchar_t[]> defaultPath = std::make_unique<wchar_t[]>(numCharsDefault);
        THROW_LAST_ERROR_IF(0 == ExpandEnvironmentStrings(DEFAULT_STARTING_DIRECTORY.c_str(), defaultPath.get(), numCharsDefault));

        return std::wstring(defaultPath.get(), numCharsDefault);
    }
}

// Method Description:
// - Helper function for converting a user-specified scrollbar state to its corresponding enum
// Arguments:
// - The value from the profiles.json file
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
ScrollbarState Profile::ParseScrollbarState(const std::wstring& scrollbarState)
{
    if (scrollbarState == AlwaysVisible)
    {
        return ScrollbarState::Visible;
    }
    else if (scrollbarState == AlwaysHide)
    {
        return ScrollbarState::Hidden;
    }
    else
    {
        return ScrollbarState::Visible;
    }
}

// Method Description:
// - Helper function for converting a user-specified image stretch mode
//   to the appropriate enum value
// Arguments:
// - The value from the profiles.json file
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
Media::Stretch Profile::ParseImageStretchMode(const std::string_view imageStretchMode)
{
    if (imageStretchMode == ImageStretchModeNone)
    {
        return Media::Stretch::None;
    }
    else if (imageStretchMode == ImageStretchModeFill)
    {
        return Media::Stretch::Fill;
    }
    else if (imageStretchMode == ImageStretchModeUniform)
    {
        return Media::Stretch::Uniform;
    }
    else // Fall through to default behavior
    {
        return Media::Stretch::UniformToFill;
    }
}

// Method Description:
// - Helper function for converting an ImageStretchMode to the
//   correct string value.
// Arguments:
// - imageStretchMode: The enum value to convert to a string.
// Return Value:
// - The string value for the given ImageStretchMode
std::string_view Profile::SerializeImageStretchMode(const Media::Stretch imageStretchMode)
{
    switch (imageStretchMode)
    {
    case Media::Stretch::None:
        return ImageStretchModeNone;
    case Media::Stretch::Fill:
        return ImageStretchModeFill;
    case Media::Stretch::Uniform:
        return ImageStretchModeUniform;
    default:
    case Media::Stretch::UniformToFill:
        return ImageStretchModeUniformTofill;
    }
}

// Method Description:
// - Helper function for converting a user-specified image horizontal and vertical
//   alignment to the appropriate enum values tuple
// Arguments:
// - The value from the profiles.json file
// Return Value:
// - The corresponding enum values tuple which maps to the string provided by the user
std::tuple<HorizontalAlignment, VerticalAlignment> Profile::ParseImageAlignment(const std::string_view imageAlignment)
{
    if (imageAlignment == ImageAlignmentTopLeft)
    {
        return std::make_tuple(HorizontalAlignment::Left,
                               VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottomLeft)
    {
        return std::make_tuple(HorizontalAlignment::Left,
                               VerticalAlignment::Bottom);
    }
    else if (imageAlignment == ImageAlignmentLeft)
    {
        return std::make_tuple(HorizontalAlignment::Left,
                               VerticalAlignment::Center);
    }
    else if (imageAlignment == ImageAlignmentTopRight)
    {
        return std::make_tuple(HorizontalAlignment::Right,
                               VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottomRight)
    {
        return std::make_tuple(HorizontalAlignment::Right,
                               VerticalAlignment::Bottom);
    }
    else if (imageAlignment == ImageAlignmentRight)
    {
        return std::make_tuple(HorizontalAlignment::Right,
                               VerticalAlignment::Center);
    }
    else if (imageAlignment == ImageAlignmentTop)
    {
        return std::make_tuple(HorizontalAlignment::Center,
                               VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottom)
    {
        return std::make_tuple(HorizontalAlignment::Center,
                               VerticalAlignment::Bottom);
    }
    else // Fall through to default alignment
    {
        return std::make_tuple(HorizontalAlignment::Center,
                               VerticalAlignment::Center);
    }
}

// Method Description:
// - Helper function for converting the HorizontalAlignment+VerticalAlignment tuple
//   to the correct string value.
// Arguments:
// - imageAlignment: The enum values tuple to convert to a string.
// Return Value:
// - The string value for the given ImageAlignment
std::string_view Profile::SerializeImageAlignment(const std::tuple<HorizontalAlignment, VerticalAlignment> imageAlignment)
{
    const auto imageHorizontalAlignment = std::get<HorizontalAlignment>(imageAlignment);
    const auto imageVerticalAlignment = std::get<VerticalAlignment>(imageAlignment);
    switch (imageHorizontalAlignment)
    {
    case HorizontalAlignment::Left:
        switch (imageVerticalAlignment)
        {
        case VerticalAlignment::Top:
            return ImageAlignmentTopLeft;
        case VerticalAlignment::Bottom:
            return ImageAlignmentBottomLeft;
        default:
        case VerticalAlignment::Center:
            return ImageAlignmentLeft;
        }

    case HorizontalAlignment::Right:
        switch (imageVerticalAlignment)
        {
        case VerticalAlignment::Top:
            return ImageAlignmentTopRight;
        case VerticalAlignment::Bottom:
            return ImageAlignmentBottomRight;
        default:
        case VerticalAlignment::Center:
            return ImageAlignmentRight;
        }

    default:
    case HorizontalAlignment::Center:
        switch (imageVerticalAlignment)
        {
        case VerticalAlignment::Top:
            return ImageAlignmentTop;
        case VerticalAlignment::Bottom:
            return ImageAlignmentBottom;
        default:
        case VerticalAlignment::Center:
            return ImageAlignmentCenter;
        }
    }
}

// Method Description:
// - Helper function for converting a user-specified cursor style corresponding
//   CursorStyle enum value
// Arguments:
// - cursorShapeString: The string value from the settings file to parse
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
CursorStyle Profile::_ParseCursorShape(const std::wstring& cursorShapeString)
{
    if (cursorShapeString == CursorShapeVintage)
    {
        return CursorStyle::Vintage;
    }
    else if (cursorShapeString == CursorShapeBar)
    {
        return CursorStyle::Bar;
    }
    else if (cursorShapeString == CursorShapeUnderscore)
    {
        return CursorStyle::Underscore;
    }
    else if (cursorShapeString == CursorShapeFilledbox)
    {
        return CursorStyle::FilledBox;
    }
    else if (cursorShapeString == CursorShapeEmptybox)
    {
        return CursorStyle::EmptyBox;
    }
    // default behavior for invalid data
    return CursorStyle::Bar;
}

// Method Description:
// - Helper function for converting a CursorStyle to its corresponding string
//   value.
// Arguments:
// - cursorShape: The enum value to convert to a string.
// Return Value:
// - The string value for the given CursorStyle
std::wstring_view Profile::_SerializeCursorStyle(const CursorStyle cursorShape)
{
    switch (cursorShape)
    {
    case CursorStyle::Underscore:
        return CursorShapeUnderscore;
    case CursorStyle::FilledBox:
        return CursorShapeFilledbox;
    case CursorStyle::EmptyBox:
        return CursorShapeEmptybox;
    case CursorStyle::Vintage:
        return CursorShapeVintage;
    default:
    case CursorStyle::Bar:
        return CursorShapeBar;
    }
}

// Method Description:
// - If this profile never had a GUID set for it, generate a runtime GUID for
//   the profile. If a profile had their guid manually set to {0}, this method
//   will _not_ change the profile's GUID.
void Profile::GenerateGuidIfNecessary() noexcept
{
    if (!_guid.has_value())
    {
        // Always use the name to generate the temporary GUID. That way, across
        // reloads, we'll generate the same static GUID.
        _guid = Profile::_GenerateGuidForProfile(_name, _source);

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "SynthesizedGuidForProfile",
            TraceLoggingDescription("Event emitted when a profile is deserialized without a GUID"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }
}

// Function Description:
// - Returns true if the given JSON object represents a dynamic profile object.
//   If it is a dynamic profile object, we should make sure to only layer the
//   object on a matching profile from a dynamic source.
// Arguments:
// - json: the partial serialization of a profile object to check
// Return Value:
// - true iff the object has a non-null `source` property
bool Profile::IsDynamicProfileObject(const Json::Value& json)
{
    const auto& source = json.isMember(JsonKey(SourceKey)) ? json[JsonKey(SourceKey)] : Json::Value::null;
    return !source.isNull();
}

// Function Description:
// - Generates a unique guid for a profile, given the name. For an given name, will always return the same GUID.
// Arguments:
// - name: The name to generate a unique GUID from
// Return Value:
// - a uuidv5 GUID generated from the given name.
GUID Profile::_GenerateGuidForProfile(const std::wstring& name, const std::optional<std::wstring>& source) noexcept
{
    // If we have a _source, then we can from a dynamic profile generator. Use
    // our source to build the naespace guid, instead of using the default GUID.

    const GUID namespaceGuid = source.has_value() ?
                                   Utils::CreateV5Uuid(RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(source.value()))) :
                                   RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID;

    // Always use the name to generate the temporary GUID. That way, across
    // reloads, we'll generate the same static GUID.
    return Utils::CreateV5Uuid(namespaceGuid, gsl::as_bytes(gsl::make_span(name)));
}

// Function Description:
// - Parses the given JSON object to get its GUID. If the json object does not
//   have a `guid` set, we'll generate one, using the `name` field.
// Arguments:
// - json: the JSON object to get a GUID from, or generate a unique GUID for
//   (given the `name`)
// Return Value:
// - The json's `guid`, or a guid synthesized for it.
GUID Profile::GetGuidOrGenerateForJson(const Json::Value& json) noexcept
{
    std::optional<GUID> guid{ std::nullopt };

    JsonUtils::GetOptionalGuid(json, GuidKey, guid);
    if (guid)
    {
        return guid.value();
    }

    const auto name = GetWstringFromJson(json[JsonKey(NameKey)]);
    std::optional<std::wstring> source{ std::nullopt };
    JsonUtils::GetOptionalString(json, SourceKey, source);

    return Profile::_GenerateGuidForProfile(name, source);
}
