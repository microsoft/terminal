// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
using namespace ::Microsoft::Console;

static constexpr std::wstring_view NAME_KEY{ L"name" };
static constexpr std::wstring_view GUID_KEY{ L"guid" };
static constexpr std::wstring_view COLORSCHEME_KEY{ L"colorScheme" };
static constexpr std::wstring_view COLORSCHEME_KEY_OLD{ L"colorscheme" };

static constexpr std::wstring_view FOREGROUND_KEY{ L"foreground" };
static constexpr std::wstring_view BACKGROUND_KEY{ L"background" };
static constexpr std::wstring_view COLORTABLE_KEY{ L"colorTable" };
static constexpr std::wstring_view HISTORYSIZE_KEY{ L"historySize" };
static constexpr std::wstring_view SNAPONINPUT_KEY{ L"snapOnInput" };
static constexpr std::wstring_view CURSORCOLOR_KEY{ L"cursorColor" };
static constexpr std::wstring_view CURSORSHAPE_KEY{ L"cursorShape" };
static constexpr std::wstring_view CURSORHEIGHT_KEY{ L"cursorHeight" };

static constexpr std::wstring_view COMMANDLINE_KEY{ L"commandline" };
static constexpr std::wstring_view FONTFACE_KEY{ L"fontFace" };
static constexpr std::wstring_view FONTSIZE_KEY{ L"fontSize" };
static constexpr std::wstring_view ACRYLICTRANSPARENCY_KEY{ L"acrylicOpacity" };
static constexpr std::wstring_view USEACRYLIC_KEY{ L"useAcrylic" };
static constexpr std::wstring_view SCROLLBARSTATE_KEY{ L"scrollbarState" };
static constexpr std::wstring_view CLOSEONEXIT_KEY{ L"closeOnExit" };
static constexpr std::wstring_view PADDING_KEY{ L"padding" };
static constexpr std::wstring_view STARTINGDIRECTORY_KEY{ L"startingDirectory" };
static constexpr std::wstring_view ICON_KEY{ L"icon" };
static constexpr std::wstring_view BACKGROUNDIMAGE_KEY{ L"backgroundImage" };
static constexpr std::wstring_view BACKGROUNDIMAGEOPACITY_KEY{ L"backgroundImageOpacity" };
static constexpr std::wstring_view BACKGROUNDIMAGESTRETCHMODE_KEY{ L"backgroundImageStretchMode" };

// Possible values for Scrollbar state
static constexpr std::wstring_view ALWAYS_VISIBLE{ L"visible" };
static constexpr std::wstring_view ALWAYS_HIDE{ L"hidden" };

// Possible values for Cursor Shape
static constexpr std::wstring_view CURSORSHAPE_VINTAGE{ L"vintage" };
static constexpr std::wstring_view CURSORSHAPE_BAR{ L"bar" };
static constexpr std::wstring_view CURSORSHAPE_UNDERSCORE{ L"underscore" };
static constexpr std::wstring_view CURSORSHAPE_FILLEDBOX{ L"filledBox" };
static constexpr std::wstring_view CURSORSHAPE_EMPTYBOX{ L"emptyBox" };

// Possible values for Image Stretch Mode
static constexpr std::wstring_view IMAGESTRETCHMODE_NONE{ L"none" };
static constexpr std::wstring_view IMAGESTRETCHMODE_FILL{ L"fill" };
static constexpr std::wstring_view IMAGESTRETCHMODE_UNIFORM{ L"uniform" };
static constexpr std::wstring_view IMAGESTRETCHMODE_UNIFORMTOFILL{ L"uniformToFill" };

Profile::Profile() :
    Profile(Utils::CreateGuid())
{
}

Profile::Profile(const winrt::guid& guid):
    _guid(guid),
    _name{ L"Default" },
    _schemeName{},

    _defaultForeground{  },
    _defaultBackground{  },
    _colorTable{},
    _historySize{ DEFAULT_HISTORY_SIZE },
    _snapOnInput{ true },
    _cursorColor{ DEFAULT_CURSOR_COLOR },
    _cursorShape{ CursorStyle::Bar },
    _cursorHeight{ DEFAULT_CURSOR_HEIGHT },

    _commandline{ L"cmd.exe" },
    _startingDirectory{  },
    _fontFace{ DEFAULT_FONT_FACE },
    _fontSize{ DEFAULT_FONT_SIZE },
    _acrylicTransparency{ 0.5 },
    _useAcrylic{ false },
    _scrollbarState{ },
    _closeOnExit{ true },
    _padding{ DEFAULT_PADDING },
    _icon{ },
    _backgroundImage{ },
    _backgroundImageOpacity{ },
    _backgroundImageStretchMode{ }
{
}

Profile::~Profile()
{

}

GUID Profile::GetGuid() const noexcept
{
    return _guid;
}

// Function Description:
// - Searches a list of color schemes to find one matching the given name. Will
//return the first match in the list, if the list has multiple schemes with the same name.
// Arguments:
// - schemes: a list of schemes to search
// - schemeName: the name of the sceme to look for
// Return Value:
// - a non-ownership pointer to the matching scheme if we found one, else nullptr
const ColorScheme* _FindScheme(const std::vector<ColorScheme>& schemes,
                               const std::wstring& schemeName)
{
    for (auto& scheme : schemes)
    {
        if (scheme.GetName() == schemeName)
        {
            return &scheme;
        }
    }
    return nullptr;
}

// Method Description:
// - Create a TerminalSettings from this object. Apply our settings, as well as
//      any colors from our color scheme, if we have one.
// Arguments:
// - schemes: a list of schemes to look for our color scheme in, if we have one.
// Return Value:
// - a new TerminalSettings object with our settings in it.
TerminalSettings Profile::CreateTerminalSettings(const std::vector<ColorScheme>& schemes) const
{
    TerminalSettings terminalSettings{};

    // Fill in the Terminal Setting's CoreSettings from the profile
    for (int i = 0; i < _colorTable.size(); i++)
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

    if (_schemeName)
    {
        const ColorScheme* const matchingScheme = _FindScheme(schemes, _schemeName.value());
        if (matchingScheme)
        {
            matchingScheme->ApplyScheme(terminalSettings);
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

    if (_scrollbarState)
    {
        ScrollbarState result = ParseScrollbarState(_scrollbarState.value());
        terminalSettings.ScrollState(result);
    }

    if (_backgroundImage)
    {
        terminalSettings.BackgroundImage(_backgroundImage.value());
    }

    if (_backgroundImageOpacity)
    {
        terminalSettings.BackgroundImageOpacity(_backgroundImageOpacity.value());
    }

    if (_backgroundImageStretchMode)
    {
        terminalSettings.BackgroundImageStretchMode(_backgroundImageStretchMode.value());
    }

    return terminalSettings;
}

// Method Description:
// - Serialize this object to a JsonObject.
// Arguments:
// - <none>
// Return Value:
// - a JsonObject which is an equivalent serialization of this object.
JsonObject Profile::ToJson() const
{
    winrt::Windows::Data::Json::JsonObject jsonObject;

    // Profile-specific settings
    const auto guidStr = Utils::GuidToString(_guid);
    const auto guid = JsonValue::CreateStringValue(guidStr);
    const auto name = JsonValue::CreateStringValue(_name);

    // Core Settings
    const auto historySize = JsonValue::CreateNumberValue(_historySize);
    const auto snapOnInput = JsonValue::CreateBooleanValue(_snapOnInput);
    const auto cursorColor = JsonValue::CreateStringValue(Utils::ColorToHexString(_cursorColor));

    // Control Settings
    const auto cmdline = JsonValue::CreateStringValue(_commandline);
    const auto fontFace = JsonValue::CreateStringValue(_fontFace);
    const auto fontSize = JsonValue::CreateNumberValue(_fontSize);
    const auto acrylicTransparency = JsonValue::CreateNumberValue(_acrylicTransparency);
    const auto useAcrylic = JsonValue::CreateBooleanValue(_useAcrylic);
    const auto closeOnExit = JsonValue::CreateBooleanValue(_closeOnExit);
    const auto padding = JsonValue::CreateStringValue(_padding);

    if (_startingDirectory)
    {
        const auto startingDirectory = JsonValue::CreateStringValue(_startingDirectory.value());
        jsonObject.Insert(STARTINGDIRECTORY_KEY, startingDirectory);
    }

    jsonObject.Insert(GUID_KEY, guid);
    jsonObject.Insert(NAME_KEY, name);

    // Core Settings
    if (_defaultForeground)
    {
        const auto defaultForeground = JsonValue::CreateStringValue(Utils::ColorToHexString(_defaultForeground.value()));
        jsonObject.Insert(FOREGROUND_KEY, defaultForeground);
    }
    if (_defaultBackground)
    {
        const auto defaultBackground = JsonValue::CreateStringValue(Utils::ColorToHexString(_defaultBackground.value()));
        jsonObject.Insert(BACKGROUND_KEY, defaultBackground);
    }
    if (_schemeName)
    {
        const auto scheme = JsonValue::CreateStringValue(_schemeName.value());
        jsonObject.Insert(COLORSCHEME_KEY, scheme);
    }
    else
    {
        JsonArray tableArray{};
        for (auto& color : _colorTable)
        {
            auto s = Utils::ColorToHexString(color);
            tableArray.Append(JsonValue::CreateStringValue(s));
        }

        jsonObject.Insert(COLORTABLE_KEY, tableArray);

    }
    jsonObject.Insert(HISTORYSIZE_KEY, historySize);
    jsonObject.Insert(SNAPONINPUT_KEY, snapOnInput);
    jsonObject.Insert(CURSORCOLOR_KEY, cursorColor);

    // Only add the cursor height property if we're a legacy-style cursor.
    if (_cursorShape == CursorStyle::Vintage)
    {
        jsonObject.Insert(CURSORHEIGHT_KEY, JsonValue::CreateNumberValue(_cursorHeight));
    }
    jsonObject.Insert(CURSORSHAPE_KEY, JsonValue::CreateStringValue(_SerializeCursorStyle(_cursorShape)));

    // Control Settings
    jsonObject.Insert(COMMANDLINE_KEY, cmdline);
    jsonObject.Insert(FONTFACE_KEY, fontFace);
    jsonObject.Insert(FONTSIZE_KEY, fontSize);
    jsonObject.Insert(ACRYLICTRANSPARENCY_KEY, acrylicTransparency);
    jsonObject.Insert(USEACRYLIC_KEY, useAcrylic);
    jsonObject.Insert(CLOSEONEXIT_KEY, closeOnExit);
    jsonObject.Insert(PADDING_KEY, padding);

    if (_scrollbarState)
    {
        const auto scrollbarState = JsonValue::CreateStringValue(_scrollbarState.value());
        jsonObject.Insert(SCROLLBARSTATE_KEY, scrollbarState);
    }

    if (_icon)
    {
        const auto icon = JsonValue::CreateStringValue(_icon.value());
        jsonObject.Insert(ICON_KEY, icon);
    }

    if (_backgroundImage)
    {
        const auto backgroundImage = JsonValue::CreateStringValue(_backgroundImage.value());
        jsonObject.Insert(BACKGROUNDIMAGE_KEY, backgroundImage);
    }

    if (_backgroundImageOpacity)
    {
        const auto opacity = JsonValue::CreateNumberValue(_backgroundImageOpacity.value());
        jsonObject.Insert(BACKGROUNDIMAGEOPACITY_KEY, opacity);
    }

    if (_backgroundImageStretchMode)
    {
        const auto imageStretchMode = JsonValue::CreateStringValue(
            SerializeImageStretchMode(_backgroundImageStretchMode.value()));
        jsonObject.Insert(BACKGROUNDIMAGESTRETCHMODE_KEY, imageStretchMode);
    }

    return jsonObject;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
Profile Profile::FromJson(winrt::Windows::Data::Json::JsonObject json)
{
    Profile result{};

    // Profile-specific Settings
    if (json.HasKey(NAME_KEY))
    {
        result._name = json.GetNamedString(NAME_KEY);
    }
    if (json.HasKey(GUID_KEY))
    {
        const auto guidString = json.GetNamedString(GUID_KEY);
        // TODO: MSFT:20737698 - if this fails, display an approriate error
        const auto guid = Utils::GuidFromString(guidString.c_str());
        result._guid = guid;
    }

    // Core Settings
    if (json.HasKey(FOREGROUND_KEY))
    {
        const auto fgString = json.GetNamedString(FOREGROUND_KEY);
        // TODO: MSFT:20737698 - if this fails, display an approriate error
        const auto color = Utils::ColorFromHexString(fgString.c_str());
        result._defaultForeground = color;
    }
    if (json.HasKey(BACKGROUND_KEY))
    {
        const auto bgString = json.GetNamedString(BACKGROUND_KEY);
        // TODO: MSFT:20737698 - if this fails, display an approriate error
        const auto color = Utils::ColorFromHexString(bgString.c_str());
        result._defaultBackground = color;
    }
    if (json.HasKey(COLORSCHEME_KEY))
    {
        result._schemeName = json.GetNamedString(COLORSCHEME_KEY);
    }
    else if (json.HasKey(COLORSCHEME_KEY_OLD))
    {
        // TODO: deprecate old settings key
        result._schemeName = json.GetNamedString(COLORSCHEME_KEY_OLD);
    }
    else if (json.HasKey(COLORTABLE_KEY))
    {
        const auto table = json.GetNamedArray(COLORTABLE_KEY);
        int i = 0;
        for (auto v : table)
        {
            if (v.ValueType() == JsonValueType::String)
            {
                const auto str = v.GetString();
                // TODO: MSFT:20737698 - if this fails, display an approriate error
                const auto color = Utils::ColorFromHexString(str.c_str());
                result._colorTable[i] = color;
            }
            i++;
        }
    }
    if (json.HasKey(HISTORYSIZE_KEY))
    {
        // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
        result._historySize = static_cast<int32_t>(json.GetNamedNumber(HISTORYSIZE_KEY));
    }
    if (json.HasKey(SNAPONINPUT_KEY))
    {
        result._snapOnInput = json.GetNamedBoolean(SNAPONINPUT_KEY);
    }
    if (json.HasKey(CURSORCOLOR_KEY))
    {
        const auto cursorString = json.GetNamedString(CURSORCOLOR_KEY);
        // TODO: MSFT:20737698 - if this fails, display an approriate error
        const auto color = Utils::ColorFromHexString(cursorString.c_str());
        result._cursorColor = color;
    }
    if (json.HasKey(CURSORHEIGHT_KEY))
    {
        result._cursorHeight = static_cast<uint32_t>(json.GetNamedNumber(CURSORHEIGHT_KEY));
    }
    if (json.HasKey(CURSORSHAPE_KEY))
    {
        const auto shapeString = json.GetNamedString(CURSORSHAPE_KEY);
        result._cursorShape = _ParseCursorShape(shapeString.c_str());
    }

    // Control Settings
    if (json.HasKey(COMMANDLINE_KEY))
    {
        result._commandline = json.GetNamedString(COMMANDLINE_KEY);
    }
    if (json.HasKey(FONTFACE_KEY))
    {
        result._fontFace = json.GetNamedString(FONTFACE_KEY);
    }
    if (json.HasKey(FONTSIZE_KEY))
    {
        result._fontSize = static_cast<int32_t>(json.GetNamedNumber(FONTSIZE_KEY));
    }
    if (json.HasKey(ACRYLICTRANSPARENCY_KEY))
    {
        result._acrylicTransparency = json.GetNamedNumber(ACRYLICTRANSPARENCY_KEY);
    }
    if (json.HasKey(USEACRYLIC_KEY))
    {
        result._useAcrylic = json.GetNamedBoolean(USEACRYLIC_KEY);
    }
    if (json.HasKey(CLOSEONEXIT_KEY))
    {
        result._closeOnExit = json.GetNamedBoolean(CLOSEONEXIT_KEY);
    }
    if (json.HasKey(PADDING_KEY))
    {
        result._padding = json.GetNamedString(PADDING_KEY);
    }
    if (json.HasKey(SCROLLBARSTATE_KEY))
    {
        result._scrollbarState = json.GetNamedString(SCROLLBARSTATE_KEY);
    }
    if (json.HasKey(STARTINGDIRECTORY_KEY))
    {
        result._startingDirectory = json.GetNamedString(STARTINGDIRECTORY_KEY);
    }
    if (json.HasKey(ICON_KEY))
    {
        result._icon = json.GetNamedString(ICON_KEY);
    }
    if (json.HasKey(BACKGROUNDIMAGE_KEY))
    {
        result._backgroundImage = json.GetNamedString(BACKGROUNDIMAGE_KEY);
    }
    if (json.HasKey(BACKGROUNDIMAGEOPACITY_KEY))
    {
        result._backgroundImageOpacity = json.GetNamedNumber(BACKGROUNDIMAGEOPACITY_KEY);
    }
    if (json.HasKey(BACKGROUNDIMAGESTRETCHMODE_KEY))
    {
        const auto stretchMode = json.GetNamedString(BACKGROUNDIMAGESTRETCHMODE_KEY);
        result._backgroundImageStretchMode = ParseImageStretchMode(stretchMode.c_str());
    }

    return result;
}



void Profile::SetFontFace(std::wstring fontFace) noexcept
{
    _fontFace = fontFace;
}

void Profile::SetColorScheme(std::optional<std::wstring> schemeName) noexcept
{
    _schemeName = schemeName;
}

void Profile::SetAcrylicOpacity(double opacity) noexcept
{
    _acrylicTransparency = opacity;
}

void Profile::SetCommandline(std::wstring cmdline) noexcept
{
    _commandline = cmdline;
}

void Profile::SetStartingDirectory(std::wstring startingDirectory) noexcept
{
    _startingDirectory = startingDirectory;
}

void Profile::SetName(std::wstring name) noexcept
{
    _name = name;
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

bool Profile::HasIcon() const noexcept
{
    return _icon.has_value();
}

// Method Description:
// - Sets this profile's icon path.
// Arguments:
// - path: the path
void Profile::SetIconPath(std::wstring_view path) noexcept
{
    _icon.emplace(path);
}

// Method Description:
// - Returns this profile's icon path, if one is set. Otherwise returns the empty string.
// Return Value:
// - this profile's icon path, if one is set. Otherwise returns the empty string.
std::wstring_view Profile::GetIconPath() const noexcept
{
    return HasIcon() ?
           std::wstring_view{ _icon.value().c_str(), _icon.value().size() } :
           std::wstring_view{ L"", 0 };
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

bool Profile::GetCloseOnExit() const noexcept
{
    return _closeOnExit;
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
    if (scrollbarState == ALWAYS_VISIBLE)
    {
        return ScrollbarState::Visible;
    }
    else if (scrollbarState == ALWAYS_HIDE)
    {
        return ScrollbarState::Hidden;
    }
    else
    {
        // default behavior for invalid data
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
winrt::Windows::UI::Xaml::Media::Stretch Profile::ParseImageStretchMode(const std::wstring& imageStretchMode)
{
    if (imageStretchMode == IMAGESTRETCHMODE_NONE)
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::None;
    }
    else if (imageStretchMode == IMAGESTRETCHMODE_FILL)
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::Fill;
    }
    else if (imageStretchMode == IMAGESTRETCHMODE_UNIFORM)
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::Uniform;
    }
    else // Fall through to default behavior
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill;
    }
}

// Method Description:
// - Helper function for converting an ImageStretchMode to the 
//   correct string value.
// Arguments:
// - imageStretchMode: The enum value to convert to a string.
// Return Value:
// - The string value for the given ImageStretchMode
std::wstring_view Profile::SerializeImageStretchMode(const winrt::Windows::UI::Xaml::Media::Stretch imageStretchMode)
{
    switch (imageStretchMode)
    {
    case winrt::Windows::UI::Xaml::Media::Stretch::None:
        return IMAGESTRETCHMODE_NONE;
    case winrt::Windows::UI::Xaml::Media::Stretch::Fill:
        return IMAGESTRETCHMODE_FILL;
    case winrt::Windows::UI::Xaml::Media::Stretch::Uniform:
        return IMAGESTRETCHMODE_UNIFORM;
    default:
    case winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill:
        return IMAGESTRETCHMODE_UNIFORMTOFILL;
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
    if (cursorShapeString == CURSORSHAPE_VINTAGE)
    {
        return CursorStyle::Vintage;
    }
    else if (cursorShapeString == CURSORSHAPE_BAR)
    {
        return CursorStyle::Bar;
    }
    else if (cursorShapeString == CURSORSHAPE_UNDERSCORE)
    {
        return CursorStyle::Underscore;
    }
    else if (cursorShapeString == CURSORSHAPE_FILLEDBOX)
    {
        return CursorStyle::FilledBox;
    }
    else if (cursorShapeString == CURSORSHAPE_EMPTYBOX)
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
            return CURSORSHAPE_UNDERSCORE;
        case CursorStyle::FilledBox:
            return CURSORSHAPE_FILLEDBOX;
        case CursorStyle::EmptyBox:
            return CURSORSHAPE_EMPTYBOX;
        case CursorStyle::Vintage:
            return CURSORSHAPE_VINTAGE;
        default:
        case CursorStyle::Bar:
            return CURSORSHAPE_BAR;
    }
}
