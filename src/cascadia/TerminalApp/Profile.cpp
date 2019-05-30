// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "Utils.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
using namespace ::Microsoft::Console;

static constexpr std::string_view NAME_KEY{ "name" };
static constexpr std::string_view GUID_KEY{ "guid" };
static constexpr std::string_view COLORSCHEME_KEY{ "colorScheme" };
static constexpr std::string_view COLORSCHEME_KEY_OLD{ "colorscheme" };

static constexpr std::string_view FOREGROUND_KEY{ "foreground" };
static constexpr std::string_view BACKGROUND_KEY{ "background" };
static constexpr std::string_view COLORTABLE_KEY{ "colorTable" };
static constexpr std::string_view HISTORYSIZE_KEY{ "historySize" };
static constexpr std::string_view SNAPONINPUT_KEY{ "snapOnInput" };
static constexpr std::string_view CURSORCOLOR_KEY{ "cursorColor" };
static constexpr std::string_view CURSORSHAPE_KEY{ "cursorShape" };
static constexpr std::string_view CURSORHEIGHT_KEY{ "cursorHeight" };

static constexpr std::string_view COMMANDLINE_KEY{ "commandline" };
static constexpr std::string_view FONTFACE_KEY{ "fontFace" };
static constexpr std::string_view FONTSIZE_KEY{ "fontSize" };
static constexpr std::string_view ACRYLICTRANSPARENCY_KEY{ "acrylicOpacity" };
static constexpr std::string_view USEACRYLIC_KEY{ "useAcrylic" };
static constexpr std::string_view SCROLLBARSTATE_KEY{ "scrollbarState" };
static constexpr std::string_view CLOSEONEXIT_KEY{ "closeOnExit" };
static constexpr std::string_view PADDING_KEY{ "padding" };
static constexpr std::string_view STARTINGDIRECTORY_KEY{ "startingDirectory" };
static constexpr std::string_view ICON_KEY{ "icon" };
static constexpr std::string_view BACKGROUNDIMAGE_KEY{ "backgroundImage" };
static constexpr std::string_view BACKGROUNDIMAGEOPACITY_KEY{ "backgroundImageOpacity" };
static constexpr std::string_view BACKGROUNDIMAGESTRETCHMODE_KEY{ "backgroundImageStretchMode" };

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
static constexpr std::string_view IMAGESTRETCHMODE_NONE{ "none" };
static constexpr std::string_view IMAGESTRETCHMODE_FILL{ "fill" };
static constexpr std::string_view IMAGESTRETCHMODE_UNIFORM{ "uniform" };
static constexpr std::string_view IMAGESTRETCHMODE_UNIFORMTOFILL{ "uniformToFill" };

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
Json::Value Profile::ToJson() const
{
    Json::Value root;

    ///// Profile-specific settings /////
    root[JsonKey(GUID_KEY)] = winrt::to_string(Utils::GuidToString(_guid));
    root[JsonKey(NAME_KEY)] = winrt::to_string(_name);

    ///// Core Settings /////
    if (_defaultForeground)
    {
        root[JsonKey(FOREGROUND_KEY)] = Utils::ColorToHexString(_defaultForeground.value());
    }
    if (_defaultBackground)
    {
        root[JsonKey(BACKGROUND_KEY)] = Utils::ColorToHexString(_defaultBackground.value());
    }
    if (_schemeName)
    {
        const auto scheme = winrt::to_string(_schemeName.value());
        root[JsonKey(COLORSCHEME_KEY)] = scheme;
    }
    else
    {
        Json::Value tableArray{};
        for (auto& color : _colorTable)
        {
            tableArray.append(Utils::ColorToHexString(color));
        }
        root[JsonKey(COLORTABLE_KEY)] = tableArray;
    }
    root[JsonKey(HISTORYSIZE_KEY)] = _historySize;
    root[JsonKey(SNAPONINPUT_KEY)] = _snapOnInput;
    root[JsonKey(CURSORCOLOR_KEY)] = Utils::ColorToHexString(_cursorColor);
    // Only add the cursor height property if we're a legacy-style cursor.
    if (_cursorShape == CursorStyle::Vintage)
    {
        root[JsonKey(CURSORHEIGHT_KEY)] = _cursorHeight;
    }
    root[JsonKey(CURSORSHAPE_KEY)] = winrt::to_string(_SerializeCursorStyle(_cursorShape));

    ///// Control Settings /////
    root[JsonKey(COMMANDLINE_KEY)] = winrt::to_string(_commandline);
    root[JsonKey(FONTFACE_KEY)] = winrt::to_string(_fontFace);
    root[JsonKey(FONTSIZE_KEY)] = _fontSize;
    root[JsonKey(ACRYLICTRANSPARENCY_KEY)] = _acrylicTransparency;
    root[JsonKey(USEACRYLIC_KEY)] = _useAcrylic;
    root[JsonKey(CLOSEONEXIT_KEY)] = _closeOnExit;
    root[JsonKey(PADDING_KEY)] = winrt::to_string(_padding);

    if (_scrollbarState)
    {
        const auto scrollbarState = winrt::to_string(_scrollbarState.value());
        root[JsonKey(SCROLLBARSTATE_KEY)] = scrollbarState;
    }

    if (_icon)
    {
        const auto icon = winrt::to_string(_icon.value());
        root[JsonKey(ICON_KEY)] = icon;
    }

    if (_startingDirectory)
    {
        root[JsonKey(STARTINGDIRECTORY_KEY)] = winrt::to_string(_startingDirectory.value());
    }

    if (_backgroundImage)
    {
        root[JsonKey(BACKGROUNDIMAGE_KEY)] = winrt::to_string(_backgroundImage.value());
    }

    if (_backgroundImageOpacity)
    {
        root[JsonKey(BACKGROUNDIMAGEOPACITY_KEY)] = _backgroundImageOpacity.value();
    }

    if (_backgroundImageStretchMode)
    {
        root[JsonKey(BACKGROUNDIMAGESTRETCHMODE_KEY)] = SerializeImageStretchMode(_backgroundImageStretchMode.value()).data();
    }

    return root;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
Profile Profile::FromJson(const Json::Value& json)
{
    Profile result{};

    // Profile-specific Settings
    if (auto name{ json[JsonKey(NAME_KEY)] })
    {
        result._name = GetWstringFromJson(name);
    }
    if (auto guid{ json[JsonKey(GUID_KEY)] })
    {
        result._guid = Utils::GuidFromString(GetWstringFromJson(guid));
    }

    // Core Settings
    if (auto foreground{ json[JsonKey(FOREGROUND_KEY)] })
    {
        const auto color = Utils::ColorFromHexString(foreground.asString());
        result._defaultForeground = color;
    }
    if (auto background{ json[JsonKey(BACKGROUND_KEY)] })
    {
        const auto color = Utils::ColorFromHexString(background.asString());
        result._defaultBackground = color;
    }
    if (auto colorScheme{ json[JsonKey(COLORSCHEME_KEY)] })
    {
        result._schemeName = GetWstringFromJson(colorScheme);
    }
    else if (auto colorScheme{ json[JsonKey(COLORSCHEME_KEY_OLD)] })
    {
        // TODO:GH#1069 deprecate old settings key
        result._schemeName = GetWstringFromJson(colorScheme);
    }
    else if (auto colortable{ json[JsonKey(COLORTABLE_KEY)] })
    {
        int i = 0;
        for (auto tableEntry : colortable)
        {
            if (tableEntry.isString())
            {
                const auto color = Utils::ColorFromHexString(tableEntry.asString());
                result._colorTable[i] = color;
            }
            i++;
        }
    }
    if (auto historySize{ json[JsonKey(HISTORYSIZE_KEY)] })
    {
        // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
        result._historySize = historySize.asInt();
    }
    if (auto snapOnInput{ json[JsonKey(SNAPONINPUT_KEY)] })
    {
        result._snapOnInput = snapOnInput.asBool();
    }
    if (auto cursorColor{ json[JsonKey(CURSORCOLOR_KEY)] })
    {
        const auto color = Utils::ColorFromHexString(cursorColor.asString());
        result._cursorColor = color;
    }
    if (auto cursorHeight{ json[JsonKey(CURSORHEIGHT_KEY)] })
    {
        result._cursorHeight = cursorHeight.asUInt();
    }
    if (auto cursorShape{ json[JsonKey(CURSORSHAPE_KEY)] })
    {
        result._cursorShape = _ParseCursorShape(GetWstringFromJson(cursorShape));
    }

    // Control Settings
    if (auto commandline{ json[JsonKey(COMMANDLINE_KEY)] })
    {
        result._commandline = GetWstringFromJson(commandline);
    }
    if (auto fontFace{ json[JsonKey(FONTFACE_KEY)] })
    {
        result._fontFace = GetWstringFromJson(fontFace);
    }
    if (auto fontSize{ json[JsonKey(FONTSIZE_KEY)] })
    {
        result._fontSize = fontSize.asInt();
    }
    if (auto acrylicTransparency{ json[JsonKey(ACRYLICTRANSPARENCY_KEY)] })
    {
        result._acrylicTransparency = acrylicTransparency.asFloat();
    }
    if (auto useAcrylic{ json[JsonKey(USEACRYLIC_KEY)] })
    {
        result._useAcrylic = useAcrylic.asBool();
    }
    if (auto closeOnExit{ json[JsonKey(CLOSEONEXIT_KEY)] })
    {
        result._closeOnExit = closeOnExit.asBool();
    }
    if (auto padding{ json[JsonKey(PADDING_KEY)] })
    {
        result._padding = GetWstringFromJson(padding);
    }
    if (auto scrollbarState{ json[JsonKey(SCROLLBARSTATE_KEY)] })
    {
        result._scrollbarState = GetWstringFromJson(scrollbarState);
    }
    if (auto startingDirectory{ json[JsonKey(STARTINGDIRECTORY_KEY)] })
    {
        result._startingDirectory = GetWstringFromJson(startingDirectory);
    }
    if (auto icon{ json[JsonKey(ICON_KEY)] })
    {
        result._icon = GetWstringFromJson(icon);
    }
    if (auto backgroundImage{ json[JsonKey(BACKGROUNDIMAGE_KEY)] })
    {
        result._backgroundImage = GetWstringFromJson(backgroundImage);
    }
    if (auto backgroundImageOpacity{ json[JsonKey(BACKGROUNDIMAGEOPACITY_KEY)] })
    {
        result._backgroundImageOpacity = backgroundImageOpacity.asFloat();
    }
    if (auto backgroundImageStretchMode{ json[JsonKey(BACKGROUNDIMAGESTRETCHMODE_KEY)] })
    {
        result._backgroundImageStretchMode = ParseImageStretchMode(backgroundImageStretchMode.asString());
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
winrt::Windows::UI::Xaml::Media::Stretch Profile::ParseImageStretchMode(const std::string& imageStretchMode)
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
std::string_view Profile::SerializeImageStretchMode(const winrt::Windows::UI::Xaml::Media::Stretch imageStretchMode)
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
