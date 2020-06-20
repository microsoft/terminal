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
static constexpr std::string_view HiddenKey{ "hidden" };

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view SelectionBackgroundKey{ "selectionBackground" };
static constexpr std::string_view TabTitleKey{ "tabTitle" };
static constexpr std::string_view SuppressApplicationTitleKey{ "suppressApplicationTitle" };
static constexpr std::string_view HistorySizeKey{ "historySize" };
static constexpr std::string_view SnapOnInputKey{ "snapOnInput" };
static constexpr std::string_view AltGrAliasingKey{ "altGrAliasing" };
static constexpr std::string_view CursorColorKey{ "cursorColor" };
static constexpr std::string_view CursorShapeKey{ "cursorShape" };
static constexpr std::string_view CursorHeightKey{ "cursorHeight" };

static constexpr std::string_view ConnectionTypeKey{ "connectionType" };
static constexpr std::string_view CommandlineKey{ "commandline" };
static constexpr std::string_view FontFaceKey{ "fontFace" };
static constexpr std::string_view FontSizeKey{ "fontSize" };
static constexpr std::string_view FontWeightKey{ "fontWeight" };
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
static constexpr std::string_view RetroTerminalEffectKey{ "experimental.retroTerminalEffect" };
static constexpr std::string_view AntialiasingModeKey{ "antialiasingMode" };

// Possible values for closeOnExit
static constexpr std::string_view CloseOnExitAlways{ "always" };
static constexpr std::string_view CloseOnExitGraceful{ "graceful" };
static constexpr std::string_view CloseOnExitNever{ "never" };

// Possible values for Scrollbar state
static constexpr std::wstring_view AlwaysVisible{ L"visible" };
static constexpr std::wstring_view AlwaysHide{ L"hidden" };

// Possible values for Cursor Shape
static constexpr std::wstring_view CursorShapeVintage{ L"vintage" };
static constexpr std::wstring_view CursorShapeBar{ L"bar" };
static constexpr std::wstring_view CursorShapeUnderscore{ L"underscore" };
static constexpr std::wstring_view CursorShapeFilledbox{ L"filledBox" };
static constexpr std::wstring_view CursorShapeEmptybox{ L"emptyBox" };

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

// Possible values for TextAntialiasingMode
static constexpr std::wstring_view AntialiasingModeGrayscale{ L"grayscale" };
static constexpr std::wstring_view AntialiasingModeCleartype{ L"cleartype" };
static constexpr std::wstring_view AntialiasingModeAliased{ L"aliased" };

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
    _cursorColor{},
    _tabTitle{},
    _suppressApplicationTitle{},
    _historySize{ DEFAULT_HISTORY_SIZE },
    _snapOnInput{ true },
    _altGrAliasing{ true },
    _cursorShape{ CursorStyle::Bar },
    _cursorHeight{ DEFAULT_CURSOR_HEIGHT },

    _connectionType{},
    _commandline{ L"cmd.exe" },
    _startingDirectory{},
    _fontFace{ DEFAULT_FONT_FACE },
    _fontSize{ DEFAULT_FONT_SIZE },
    /* _fontWeight is initialized below because the structure won't accept a uint16_t directly */
    _acrylicTransparency{ 0.5 },
    _useAcrylic{ false },
    _scrollbarState{},
    _closeOnExitMode{ CloseOnExitMode::Graceful },
    _padding{ DEFAULT_PADDING },
    _icon{},
    _backgroundImage{},
    _backgroundImageOpacity{},
    _backgroundImageStretchMode{},
    _backgroundImageAlignment{},
    _retroTerminalEffect{},
    _antialiasingMode{ TextAntialiasingMode::Grayscale }
{
    winrt::Windows::UI::Text::FontWeight weight;
    weight.Weight = DEFAULT_FONT_WEIGHT;
    _fontWeight = weight;
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

GUID Profile::GetGuid() const
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
    terminalSettings.HistorySize(_historySize);
    terminalSettings.SnapOnInput(_snapOnInput);
    terminalSettings.AltGrAliasing(_altGrAliasing);
    terminalSettings.CursorHeight(_cursorHeight);
    terminalSettings.CursorShape(_cursorShape);

    // Fill in the remaining properties from the profile
    terminalSettings.ProfileName(_name);
    terminalSettings.UseAcrylic(_useAcrylic);
    terminalSettings.TintOpacity(_acrylicTransparency);

    terminalSettings.FontFace(_fontFace);
    terminalSettings.FontSize(_fontSize);
    terminalSettings.FontWeight(_fontWeight);
    terminalSettings.Padding(_padding);

    terminalSettings.Commandline(_commandline);

    if (_startingDirectory)
    {
        const auto evaluatedDirectory = Profile::EvaluateStartingDirectory(_startingDirectory.value());
        terminalSettings.StartingDirectory(evaluatedDirectory);
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
    if (_cursorColor)
    {
        terminalSettings.CursorColor(_cursorColor.value());
    }

    if (_scrollbarState)
    {
        ScrollbarState result = ParseScrollbarState(_scrollbarState.value());
        terminalSettings.ScrollState(result);
    }

    if (HasBackgroundImage())
    {
        terminalSettings.BackgroundImage(GetExpandedBackgroundImagePath());
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

    if (_retroTerminalEffect)
    {
        terminalSettings.RetroTerminalEffect(_retroTerminalEffect.value());
    }

    terminalSettings.AntialiasingMode(_antialiasingMode);

    return terminalSettings;
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
// - Helper function to convert a json value into a bool.
//   Used with JsonUtils::GetOptionalValue.
// Arguments:
// - json: the Json::Value object to parse.
// Return Value:
// - A bool
bool Profile::_ConvertJsonToBool(const Json::Value& json)
{
    return json.asBool();
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
    JsonUtils::GetWstring(json, NameKey, _name);

    JsonUtils::GetOptionalGuid(json, GuidKey, _guid);

    JsonUtils::GetBool(json, HiddenKey, _hidden);

    // Core Settings
    JsonUtils::GetOptionalColor(json, ForegroundKey, _defaultForeground);

    JsonUtils::GetOptionalColor(json, BackgroundKey, _defaultBackground);

    JsonUtils::GetOptionalColor(json, SelectionBackgroundKey, _selectionBackground);

    JsonUtils::GetOptionalColor(json, CursorColorKey, _cursorColor);

    JsonUtils::GetOptionalString(json, ColorSchemeKey, _schemeName);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::GetInt(json, HistorySizeKey, _historySize);

    JsonUtils::GetBool(json, SnapOnInputKey, _snapOnInput);

    JsonUtils::GetBool(json, AltGrAliasingKey, _altGrAliasing);

    JsonUtils::GetUInt(json, CursorHeightKey, _cursorHeight);

    if (json.isMember(JsonKey(CursorShapeKey)))
    {
        auto cursorShape{ json[JsonKey(CursorShapeKey)] };
        _cursorShape = _ParseCursorShape(GetWstringFromJson(cursorShape));
    }
    JsonUtils::GetOptionalString(json, TabTitleKey, _tabTitle);

    // Control Settings
    JsonUtils::GetOptionalGuid(json, ConnectionTypeKey, _connectionType);

    JsonUtils::GetWstring(json, CommandlineKey, _commandline);

    JsonUtils::GetWstring(json, FontFaceKey, _fontFace);

    JsonUtils::GetInt(json, FontSizeKey, _fontSize);

    if (json.isMember(JsonKey(FontWeightKey)))
    {
        auto fontWeight{ json[JsonKey(FontWeightKey)] };
        _fontWeight = _ParseFontWeight(fontWeight);
    }

    JsonUtils::GetDouble(json, AcrylicTransparencyKey, _acrylicTransparency);

    JsonUtils::GetBool(json, UseAcrylicKey, _useAcrylic);

    JsonUtils::GetBool(json, SuppressApplicationTitleKey, _suppressApplicationTitle);

    if (json.isMember(JsonKey(CloseOnExitKey)))
    {
        auto closeOnExit{ json[JsonKey(CloseOnExitKey)] };
        _closeOnExitMode = ParseCloseOnExitMode(closeOnExit);
    }

    JsonUtils::GetWstring(json, PaddingKey, _padding);

    JsonUtils::GetOptionalString(json, ScrollbarStateKey, _scrollbarState);

    JsonUtils::GetOptionalString(json, StartingDirectoryKey, _startingDirectory);

    JsonUtils::GetOptionalString(json, IconKey, _icon);

    JsonUtils::GetOptionalString(json, BackgroundImageKey, _backgroundImage);

    JsonUtils::GetOptionalDouble(json, BackgroundImageOpacityKey, _backgroundImageOpacity);

    JsonUtils::GetOptionalValue(json, BackgroundImageStretchModeKey, _backgroundImageStretchMode, &Profile::_ConvertJsonToStretchMode);

    JsonUtils::GetOptionalValue(json, BackgroundImageAlignmentKey, _backgroundImageAlignment, &Profile::_ConvertJsonToAlignment);

    JsonUtils::GetOptionalValue(json, RetroTerminalEffectKey, _retroTerminalEffect, Profile::_ConvertJsonToBool);

    if (json.isMember(JsonKey(AntialiasingModeKey)))
    {
        auto antialiasingMode{ json[JsonKey(AntialiasingModeKey)] };
        _antialiasingMode = ParseTextAntialiasingMode(GetWstringFromJson(antialiasingMode));
    }
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

void Profile::SetName(const std::wstring_view name) noexcept
{
    _name = static_cast<std::wstring>(name);
}

void Profile::SetUseAcrylic(bool useAcrylic) noexcept
{
    _useAcrylic = useAcrylic;
}

void Profile::SetDefaultForeground(til::color defaultForeground) noexcept
{
    _defaultForeground = defaultForeground;
}

void Profile::SetDefaultBackground(til::color defaultBackground) noexcept
{
    _defaultBackground = defaultBackground;
}

void Profile::SetSelectionBackground(til::color selectionBackground) noexcept
{
    _selectionBackground = selectionBackground;
}

void Profile::SetCloseOnExitMode(CloseOnExitMode mode) noexcept
{
    _closeOnExitMode = mode;
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
// - Resets the std::optional holding the icon file path string.
//   HasIcon() will return false after the execution of this function.
void Profile::ResetIconPath()
{
    _icon.reset();
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
// - Resets the std::optional holding the background image file path string.
//   HasBackgroundImage() will return false after the execution of this function.
void Profile::ResetBackgroundImagePath()
{
    _backgroundImage.reset();
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

CloseOnExitMode Profile::GetCloseOnExitMode() const noexcept
{
    return _closeOnExitMode;
}

// Method Description:
// - If a profile is marked hidden, it should not appear in the dropdown list of
//   profiles. This setting is used to "remove" default and dynamic profiles
//   from the list of profiles.
// Arguments:
// - <none>
// Return Value:
// - true iff the profile should be hidden from the list of profiles.
bool Profile::IsHidden() const noexcept
{
    return _hidden;
}

// Method Description:
// - Helper function for expanding any environment variables in a user-supplied starting directory and validating the resulting path
// Arguments:
// - The value from the settings.json file
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
// - Helper function for converting a user-specified font weight value to its corresponding enum
// Arguments:
// - The value from the settings.json file
// Return Value:
// - The corresponding value which maps to the string provided by the user
winrt::Windows::UI::Text::FontWeight Profile::_ParseFontWeight(const Json::Value& json)
{
    if (json.isUInt())
    {
        winrt::Windows::UI::Text::FontWeight weight;
        weight.Weight = static_cast<uint16_t>(json.asUInt());

        // We're only accepting variable values between 100 and 990 so we don't go too crazy.
        if (weight.Weight >= 100 && weight.Weight <= 990)
        {
            return weight;
        }
    }

    if (json.isString())
    {
        auto fontWeight = json.asString();
        if (fontWeight == FontWeightThin)
        {
            return winrt::Windows::UI::Text::FontWeights::Thin();
        }
        else if (fontWeight == FontWeightExtraLight)
        {
            return winrt::Windows::UI::Text::FontWeights::ExtraLight();
        }
        else if (fontWeight == FontWeightLight)
        {
            return winrt::Windows::UI::Text::FontWeights::Light();
        }
        else if (fontWeight == FontWeightSemiLight)
        {
            return winrt::Windows::UI::Text::FontWeights::SemiLight();
        }
        else if (fontWeight == FontWeightNormal)
        {
            return winrt::Windows::UI::Text::FontWeights::Normal();
        }
        else if (fontWeight == FontWeightMedium)
        {
            return winrt::Windows::UI::Text::FontWeights::Medium();
        }
        else if (fontWeight == FontWeightSemiBold)
        {
            return winrt::Windows::UI::Text::FontWeights::SemiBold();
        }
        else if (fontWeight == FontWeightBold)
        {
            return winrt::Windows::UI::Text::FontWeights::Bold();
        }
        else if (fontWeight == FontWeightExtraBold)
        {
            return winrt::Windows::UI::Text::FontWeights::ExtraBold();
        }
        else if (fontWeight == FontWeightBlack)
        {
            return winrt::Windows::UI::Text::FontWeights::Black();
        }
        else if (fontWeight == FontWeightExtraBlack)
        {
            return winrt::Windows::UI::Text::FontWeights::ExtraBlack();
        }
    }

    return winrt::Windows::UI::Text::FontWeights::Normal();
}

// Method Description:
// - Helper function for converting a user-specified closeOnExit value to its corresponding enum
// Arguments:
// - The value from the settings.json file
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
CloseOnExitMode Profile::ParseCloseOnExitMode(const Json::Value& json)
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

// Method Description:
// - Helper function for converting a user-specified scrollbar state to its corresponding enum
// Arguments:
// - The value from the settings.json file
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
// - The value from the settings.json file
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
// - Helper function for converting a user-specified image horizontal and vertical
//   alignment to the appropriate enum values tuple
// Arguments:
// - The value from the settings.json file
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
    // our source to build the namespace guid, instead of using the default GUID.

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

void Profile::SetRetroTerminalEffect(bool value) noexcept
{
    _retroTerminalEffect = value;
}

// Method Description:
// - Helper function for converting a user-specified antialiasing mode
//   corresponding TextAntialiasingMode enum value
// Arguments:
// - antialiasingMode: The string value from the settings file to parse
// Return Value:
// - The corresponding enum value which maps to the string provided by the user
TextAntialiasingMode Profile::ParseTextAntialiasingMode(const std::wstring& antialiasingMode)
{
    if (antialiasingMode == AntialiasingModeCleartype)
    {
        return TextAntialiasingMode::Cleartype;
    }
    else if (antialiasingMode == AntialiasingModeAliased)
    {
        return TextAntialiasingMode::Aliased;
    }
    else if (antialiasingMode == AntialiasingModeGrayscale)
    {
        return TextAntialiasingMode::Grayscale;
    }
    // default behavior for invalid data
    return TextAntialiasingMode::Grayscale;
}
