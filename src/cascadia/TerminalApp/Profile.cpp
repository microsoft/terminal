// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "Utils.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

using namespace TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::Microsoft::Console;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view GuidKey{ "guid" };
static constexpr std::string_view ColorSchemeKey{ "colorScheme" };
static constexpr std::string_view ColorSchemeKeyOld{ "colorscheme" };

static constexpr std::string_view ForegroundKey{ "foreground" };
static constexpr std::string_view BackgroundKey{ "background" };
static constexpr std::string_view ColorTableKey{ "colorTable" };
static constexpr std::string_view TabTitleKey{ "tabTitle" };
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
    Profile(Utils::CreateGuid())
{
}

Profile::Profile(const winrt::guid& guid) :
    _guid(guid),
    _name{ L"Default" },
    _schemeName{},

    _defaultForeground{},
    _defaultBackground{},
    _colorTable{},
    _tabTitle{},
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

    if (_backgroundImageAlignment)
    {
        const auto imageHorizontalAlignment = std::get<winrt::Windows::UI::Xaml::HorizontalAlignment>(_backgroundImageAlignment.value());
        const auto imageVerticalAlignment = std::get<winrt::Windows::UI::Xaml::VerticalAlignment>(_backgroundImageAlignment.value());
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
    Json::Value root;

    ///// Profile-specific settings /////
    root[JsonKey(GuidKey)] = winrt::to_string(Utils::GuidToString(_guid));
    root[JsonKey(NameKey)] = winrt::to_string(_name);

    ///// Core Settings /////
    if (_defaultForeground)
    {
        root[JsonKey(ForegroundKey)] = Utils::ColorToHexString(_defaultForeground.value());
    }
    if (_defaultBackground)
    {
        root[JsonKey(BackgroundKey)] = Utils::ColorToHexString(_defaultBackground.value());
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
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
Profile Profile::FromJson(const Json::Value& json)
{
    Profile result{};

    // Profile-specific Settings
    if (auto name{ json[JsonKey(NameKey)] })
    {
        result._name = GetWstringFromJson(name);
    }
    if (auto guid{ json[JsonKey(GuidKey)] })
    {
        result._guid = Utils::GuidFromString(GetWstringFromJson(guid));
    }
    else
    {
        result._guid = Utils::CreateGuid();

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "SynthesizedGuidForProfile",
            TraceLoggingDescription("Event emitted when a profile is deserialized without a GUID"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }

    // Core Settings
    if (auto foreground{ json[JsonKey(ForegroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(foreground.asString());
        result._defaultForeground = color;
    }
    if (auto background{ json[JsonKey(BackgroundKey)] })
    {
        const auto color = Utils::ColorFromHexString(background.asString());
        result._defaultBackground = color;
    }
    if (auto colorScheme{ json[JsonKey(ColorSchemeKey)] })
    {
        result._schemeName = GetWstringFromJson(colorScheme);
    }
    else if (auto colorScheme{ json[JsonKey(ColorSchemeKeyOld)] })
    {
        // TODO:GH#1069 deprecate old settings key
        result._schemeName = GetWstringFromJson(colorScheme);
    }
    else if (auto colortable{ json[JsonKey(ColorTableKey)] })
    {
        int i = 0;
        for (const auto& tableEntry : colortable)
        {
            if (tableEntry.isString())
            {
                const auto color = Utils::ColorFromHexString(tableEntry.asString());
                result._colorTable[i] = color;
            }
            i++;
        }
    }
    if (auto historySize{ json[JsonKey(HistorySizeKey)] })
    {
        // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
        result._historySize = historySize.asInt();
    }
    if (auto snapOnInput{ json[JsonKey(SnapOnInputKey)] })
    {
        result._snapOnInput = snapOnInput.asBool();
    }
    if (auto cursorColor{ json[JsonKey(CursorColorKey)] })
    {
        const auto color = Utils::ColorFromHexString(cursorColor.asString());
        result._cursorColor = color;
    }
    if (auto cursorHeight{ json[JsonKey(CursorHeightKey)] })
    {
        result._cursorHeight = cursorHeight.asUInt();
    }
    if (auto cursorShape{ json[JsonKey(CursorShapeKey)] })
    {
        result._cursorShape = _ParseCursorShape(GetWstringFromJson(cursorShape));
    }
    if (auto tabTitle{ json[JsonKey(TabTitleKey)] })
    {
        result._tabTitle = GetWstringFromJson(tabTitle);
    }

    // Control Settings
    if (auto connectionType{ json[JsonKey(ConnectionTypeKey)] })
    {
        result._connectionType = Utils::GuidFromString(GetWstringFromJson(connectionType));
    }
    if (auto commandline{ json[JsonKey(CommandlineKey)] })
    {
        result._commandline = GetWstringFromJson(commandline);
    }
    if (auto fontFace{ json[JsonKey(FontFaceKey)] })
    {
        result._fontFace = GetWstringFromJson(fontFace);
    }
    if (auto fontSize{ json[JsonKey(FontSizeKey)] })
    {
        result._fontSize = fontSize.asInt();
    }
    if (auto acrylicTransparency{ json[JsonKey(AcrylicTransparencyKey)] })
    {
        result._acrylicTransparency = acrylicTransparency.asFloat();
    }
    if (auto useAcrylic{ json[JsonKey(UseAcrylicKey)] })
    {
        result._useAcrylic = useAcrylic.asBool();
    }
    if (auto closeOnExit{ json[JsonKey(CloseOnExitKey)] })
    {
        result._closeOnExit = closeOnExit.asBool();
    }
    if (auto padding{ json[JsonKey(PaddingKey)] })
    {
        result._padding = GetWstringFromJson(padding);
    }
    if (auto scrollbarState{ json[JsonKey(ScrollbarStateKey)] })
    {
        result._scrollbarState = GetWstringFromJson(scrollbarState);
    }
    if (auto startingDirectory{ json[JsonKey(StartingDirectoryKey)] })
    {
        result._startingDirectory = GetWstringFromJson(startingDirectory);
    }
    if (auto icon{ json[JsonKey(IconKey)] })
    {
        result._icon = GetWstringFromJson(icon);
    }
    if (auto backgroundImage{ json[JsonKey(BackgroundImageKey)] })
    {
        result._backgroundImage = GetWstringFromJson(backgroundImage);
    }
    if (auto backgroundImageOpacity{ json[JsonKey(BackgroundImageOpacityKey)] })
    {
        result._backgroundImageOpacity = backgroundImageOpacity.asFloat();
    }
    if (auto backgroundImageStretchMode{ json[JsonKey(BackgroundImageStretchModeKey)] })
    {
        result._backgroundImageStretchMode = ParseImageStretchMode(backgroundImageStretchMode.asString());
    }
    if (auto backgroundImageAlignment{ json[JsonKey(BackgroundImageAlignmentKey)] })
    {
        result._backgroundImageAlignment = ParseImageAlignment(backgroundImageAlignment.asString());
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

// Method Description
// - Sets this profile's tab title.
// Arguments:
// - tabTitle: the tab title
void Profile::SetTabTitle(std::wstring tabTitle) noexcept
{
    _tabTitle = tabTitle;
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

// Method Description:
// - Returns true if profile's custom tab title is set, if one is set. Otherwise returns false.
// Return Value:
// - true if this profile's custom tab title is set. Otherwise returns false.
bool Profile::HasTabTitle() const noexcept
{
    return _tabTitle.has_value();
}

// Method Description:
// - Returns the custom tab title, if one is set. Otherwise returns the empty string.
// Return Value:
// - this profile's custom tab title, if one is set. Otherwise returns the empty string.
std::wstring_view Profile::GetTabTitle() const noexcept
{
    return HasTabTitle() ?
               std::wstring_view{ _tabTitle.value().c_str(), _tabTitle.value().size() } :
               std::wstring_view{ L"", 0 };
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
winrt::Windows::UI::Xaml::Media::Stretch Profile::ParseImageStretchMode(const std::string_view imageStretchMode)
{
    if (imageStretchMode == ImageStretchModeNone)
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::None;
    }
    else if (imageStretchMode == ImageStretchModeFill)
    {
        return winrt::Windows::UI::Xaml::Media::Stretch::Fill;
    }
    else if (imageStretchMode == ImageStretchModeUniform)
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
        return ImageStretchModeNone;
    case winrt::Windows::UI::Xaml::Media::Stretch::Fill:
        return ImageStretchModeFill;
    case winrt::Windows::UI::Xaml::Media::Stretch::Uniform:
        return ImageStretchModeUniform;
    default:
    case winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill:
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
std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment> Profile::ParseImageAlignment(const std::string_view imageAlignment)
{
    if (imageAlignment == ImageAlignmentTopLeft)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Left,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottomLeft)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Left,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Bottom);
    }
    else if (imageAlignment == ImageAlignmentLeft)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Left,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Center);
    }
    else if (imageAlignment == ImageAlignmentTopRight)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Right,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottomRight)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Right,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Bottom);
    }
    else if (imageAlignment == ImageAlignmentRight)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Right,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Center);
    }
    else if (imageAlignment == ImageAlignmentTop)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Center,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Top);
    }
    else if (imageAlignment == ImageAlignmentBottom)
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Center,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Bottom);
    }
    else // Fall through to default alignment
    {
        return std::make_tuple(winrt::Windows::UI::Xaml::HorizontalAlignment::Center,
                               winrt::Windows::UI::Xaml::VerticalAlignment::Center);
    }
}

// Method Description:
// - Helper function for converting the HorizontalAlignment+VerticalAlignment tuple
//   to the correct string value.
// Arguments:
// - imageAlignment: The enum values tuple to convert to a string.
// Return Value:
// - The string value for the given ImageAlignment
std::string_view Profile::SerializeImageAlignment(const std::tuple<winrt::Windows::UI::Xaml::HorizontalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment> imageAlignment)
{
    const auto imageHorizontalAlignment = std::get<winrt::Windows::UI::Xaml::HorizontalAlignment>(imageAlignment);
    const auto imageVerticalAlignment = std::get<winrt::Windows::UI::Xaml::VerticalAlignment>(imageAlignment);
    switch (imageHorizontalAlignment)
    {
    case winrt::Windows::UI::Xaml::HorizontalAlignment::Left:
        switch (imageVerticalAlignment)
        {
        case winrt::Windows::UI::Xaml::VerticalAlignment::Top:
            return ImageAlignmentTopLeft;
        case winrt::Windows::UI::Xaml::VerticalAlignment::Bottom:
            return ImageAlignmentBottomLeft;
        default:
        case winrt::Windows::UI::Xaml::VerticalAlignment::Center:
            return ImageAlignmentLeft;
        }

    case winrt::Windows::UI::Xaml::HorizontalAlignment::Right:
        switch (imageVerticalAlignment)
        {
        case winrt::Windows::UI::Xaml::VerticalAlignment::Top:
            return ImageAlignmentTopRight;
        case winrt::Windows::UI::Xaml::VerticalAlignment::Bottom:
            return ImageAlignmentBottomRight;
        default:
        case winrt::Windows::UI::Xaml::VerticalAlignment::Center:
            return ImageAlignmentRight;
        }

    default:
    case winrt::Windows::UI::Xaml::HorizontalAlignment::Center:
        switch (imageVerticalAlignment)
        {
        case winrt::Windows::UI::Xaml::VerticalAlignment::Top:
            return ImageAlignmentTop;
        case winrt::Windows::UI::Xaml::VerticalAlignment::Bottom:
            return ImageAlignmentBottom;
        default:
        case winrt::Windows::UI::Xaml::VerticalAlignment::Center:
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
