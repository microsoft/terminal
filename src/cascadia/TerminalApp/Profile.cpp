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
static constexpr std::string_view COLORSCHEME_KEY{ "colorscheme" };

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

// Possible values for Scrollbar state
static constexpr std::wstring_view ALWAYS_VISIBLE{ L"visible" };
static constexpr std::wstring_view ALWAYS_HIDE{ L"hidden" };

// Possible values for Cursor Shape
static constexpr std::wstring_view CURSORSHAPE_VINTAGE{ L"vintage" };
static constexpr std::wstring_view CURSORSHAPE_BAR{ L"bar" };
static constexpr std::wstring_view CURSORSHAPE_UNDERSCORE{ L"underscore" };
static constexpr std::wstring_view CURSORSHAPE_FILLEDBOX{ L"filledBox" };
static constexpr std::wstring_view CURSORSHAPE_EMPTYBOX{ L"emptyBox" };

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
    _icon{ }
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
//      any colors from our colorscheme, if we have one.
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
    root[GUID_KEY.data()] = winrt::to_string(Utils::GuidToString(_guid));
    root[NAME_KEY.data()] = winrt::to_string(_name);

    ///// Core Settings /////
    if (_defaultForeground)
    {
        const auto defaultForeground = winrt::to_string(Utils::ColorToHexString(_defaultForeground.value()));
        root[FOREGROUND_KEY.data()] = defaultForeground;
    }
    if (_defaultBackground)
    {
        const auto defaultBackground = winrt::to_string(Utils::ColorToHexString(_defaultBackground.value()));
        root[BACKGROUND_KEY.data()] = defaultBackground;
    }
    if (_schemeName)
    {
        const auto scheme = winrt::to_string(_schemeName.value());
        root[COLORSCHEME_KEY.data()] = scheme;
    }
    else
    {
        Json::Value tableArray{};
        for (auto& color : _colorTable)
        {
            auto s = Utils::ColorToHexString(color);
            tableArray.append(winrt::to_string(s));
        }
        root[COLORTABLE_KEY.data()] = tableArray;
    }
    root[HISTORYSIZE_KEY.data()] = _historySize;
    root[SNAPONINPUT_KEY.data()] = _snapOnInput;
    root[CURSORCOLOR_KEY.data()] = winrt::to_string(Utils::ColorToHexString(_cursorColor));
    // Only add the cursor height property if we're a legacy-style cursor.
    if (_cursorShape == CursorStyle::Vintage)
    {
        root[CURSORHEIGHT_KEY.data()] = _cursorHeight;
    }
    root[CURSORSHAPE_KEY.data()] = winrt::to_string(_SerializeCursorStyle(_cursorShape));

    ///// Control Settings /////
    root[COMMANDLINE_KEY.data()] = winrt::to_string(_commandline);
    root[FONTFACE_KEY.data()] = winrt::to_string(_fontFace);
    root[FONTSIZE_KEY.data()] = _fontSize;
    root[ACRYLICTRANSPARENCY_KEY.data()] = _acrylicTransparency;
    root[USEACRYLIC_KEY.data()] = _useAcrylic;
    root[CLOSEONEXIT_KEY.data()] = _closeOnExit;
    root[PADDING_KEY.data()] = winrt::to_string(_padding);

    if (_scrollbarState)
    {
        const auto scrollbarState = winrt::to_string(_scrollbarState.value());
        root[SCROLLBARSTATE_KEY.data()] = scrollbarState;
    }

    if (_icon)
    {
        const auto icon = winrt::to_string(_icon.value());
        root[ICON_KEY.data()] = icon;
    }

    if (_startingDirectory)
    {
        root[STARTINGDIRECTORY_KEY.data()] = winrt::to_string(_startingDirectory.value());
    }

    return root;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
Profile Profile::FromJson(Json::Value json)
{
    Profile result{};

    // Profile-specific Settings
    if (auto name{ json[NAME_KEY.data()] })
    {
        result._name = GetWstringFromJson(name);
    }
    if (auto guid{ json[GUID_KEY.data()] })
    {
        result._guid = Utils::GuidFromString(GetWstringFromJson(guid));
    }

    // Core Settings
    if (auto foreground{ json[FOREGROUND_KEY.data()] })
    {
        const auto color = Utils::ColorFromHexString(GetWstringFromJson(foreground));
        result._defaultForeground = color;
    }
    if (auto background{ json[BACKGROUND_KEY.data()] })
    {
        const auto color = Utils::ColorFromHexString(GetWstringFromJson(background));
        result._defaultBackground = color;
    }
    if (auto colorScheme{ json[COLORSCHEME_KEY.data()] })
    {
        result._schemeName = GetWstringFromJson(colorScheme);
    }
    else
    {
        if (auto colortable{ json[COLORTABLE_KEY.data()] })
        {
            int i = 0;
            for (auto tableEntry : colortable)
            {
                if (tableEntry.isString())
                {
                    const auto color = Utils::ColorFromHexString(GetWstringFromJson(tableEntry));
                    result._colorTable[i] = color;
                }
                i++;
            }
        }
    }
    if (auto historySize{ json[HISTORYSIZE_KEY.data()] })
    {
        // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
        result._historySize = historySize.asInt();
    }
    if (auto snapOnInput{ json[SNAPONINPUT_KEY.data()] })
    {
        result._snapOnInput = snapOnInput.asBool();
    }
    if (auto cursorColor{ json[CURSORCOLOR_KEY.data()] })
    {
        const auto color = Utils::ColorFromHexString(GetWstringFromJson(cursorColor));
        result._cursorColor = color;
    }
    if (auto cursorHeight{ json[CURSORHEIGHT_KEY.data()] })
    {
        result._cursorHeight = cursorHeight.asUInt();
    }
    if (auto cursorShape{ json[CURSORSHAPE_KEY.data()] })
    {
        result._cursorShape = _ParseCursorShape(GetWstringFromJson(cursorShape));
    }

    // Control Settings
    if (auto commandline{ json[COMMANDLINE_KEY.data()] })
    {
        result._commandline = GetWstringFromJson(commandline);
    }
    if (auto fontFace{ json[FONTFACE_KEY.data()] })
    {
        result._fontFace = GetWstringFromJson(fontFace);
    }
    if (auto fontSize{ json[FONTSIZE_KEY.data()] })
    {
        result._fontSize = fontSize.asInt();
    }
    if (auto acrylicTransparency{ json[ACRYLICTRANSPARENCY_KEY.data()] })
    {
        result._acrylicTransparency = acrylicTransparency.asFloat();
    }
    if (auto useAcrylic{ json[USEACRYLIC_KEY.data()] })
    {
        result._useAcrylic = useAcrylic.asBool();
    }
    if (auto closeOnExit{ json[CLOSEONEXIT_KEY.data()] })
    {
        result._closeOnExit = closeOnExit.asBool();
    }
    if (auto padding{ json[PADDING_KEY.data()] })
    {
        result._padding = GetWstringFromJson(padding);
    }
    if (auto scrollbarState{ json[SCROLLBARSTATE_KEY.data()] })
    {
        result._scrollbarState = GetWstringFromJson(scrollbarState);
    }
    if (auto startingDirectory{ json[STARTINGDIRECTORY_KEY.data()] })
    {
        result._startingDirectory = GetWstringFromJson(startingDirectory);
    }
    if (auto icon{ json[ICON_KEY.data()] })
    {
        result._icon = GetWstringFromJson(icon);
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
