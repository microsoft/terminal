// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

#include "LegacyProfileGeneratorNamespaces.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "Profile.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::Foundation;
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
static constexpr std::string_view TabColorKey{ "tabColor" };
static constexpr std::string_view BellStyleKey{ "bellStyle" };
static constexpr std::string_view PixelShaderPathKey{ "experimental.pixelShaderPath" };

static constexpr std::wstring_view DesktopWallpaperEnum{ L"desktopWallpaper" };

Profile::Profile()
{
}

Profile::Profile(guid guid) :
    _Guid(guid)
{
}

winrt::com_ptr<Profile> Profile::CopySettings(winrt::com_ptr<Profile> source)
{
    auto profile{ winrt::make_self<Profile>() };
    profile->_Guid = source->_Guid;
    profile->_Name = source->_Name;
    profile->_Source = source->_Source;
    profile->_Hidden = source->_Hidden;
    profile->_Icon = source->_Icon;
    profile->_CloseOnExit = source->_CloseOnExit;
    profile->_TabTitle = source->_TabTitle;
    profile->_TabColor = source->_TabColor;
    profile->_SuppressApplicationTitle = source->_SuppressApplicationTitle;
    profile->_UseAcrylic = source->_UseAcrylic;
    profile->_AcrylicOpacity = source->_AcrylicOpacity;
    profile->_ScrollState = source->_ScrollState;
    profile->_FontFace = source->_FontFace;
    profile->_FontSize = source->_FontSize;
    profile->_FontWeight = source->_FontWeight;
    profile->_Padding = source->_Padding;
    profile->_Commandline = source->_Commandline;
    profile->_StartingDirectory = source->_StartingDirectory;
    profile->_BackgroundImagePath = source->_BackgroundImagePath;
    profile->_BackgroundImageOpacity = source->_BackgroundImageOpacity;
    profile->_BackgroundImageStretchMode = source->_BackgroundImageStretchMode;
    profile->_AntialiasingMode = source->_AntialiasingMode;
    profile->_RetroTerminalEffect = source->_RetroTerminalEffect;
    profile->_ForceFullRepaintRendering = source->_ForceFullRepaintRendering;
    profile->_SoftwareRendering = source->_SoftwareRendering;
    profile->_ColorSchemeName = source->_ColorSchemeName;
    profile->_Foreground = source->_Foreground;
    profile->_Background = source->_Background;
    profile->_SelectionBackground = source->_SelectionBackground;
    profile->_CursorColor = source->_CursorColor;
    profile->_HistorySize = source->_HistorySize;
    profile->_SnapOnInput = source->_SnapOnInput;
    profile->_AltGrAliasing = source->_AltGrAliasing;
    profile->_CursorShape = source->_CursorShape;
    profile->_CursorHeight = source->_CursorHeight;
    profile->_BellStyle = source->_BellStyle;
    profile->_PixelShaderPath = source->_PixelShaderPath;
    profile->_BackgroundImageAlignment = source->_BackgroundImageAlignment;
    profile->_ConnectionType = source->_ConnectionType;

    return profile;
}

// Method Description:
// - Creates a copy of the inheritance graph by performing a depth-first traversal recursively.
//   Profiles are recorded as visited via the "visited" parameter.
//   Unvisited Profiles are copied into the "cloneGraph" parameter, then marked as visited.
// Arguments:
// - sourceGraph - the graph of Profile's we're cloning
// - cloneGraph - the clone of sourceGraph that is being constructed
// - visited - a map of which Profiles have been visited, and, if so, a reference to the Profile's clone
// Return Value:
// - a clone in both inheritance structure and Profile values of sourceGraph
winrt::com_ptr<Profile> Profile::CloneInheritanceGraph(winrt::com_ptr<Profile> sourceGraph, winrt::com_ptr<Profile> cloneGraph, std::unordered_map<void*, winrt::com_ptr<Profile>>& visited)
{
    // If this is an unexplored Profile
    //   and we have parents...
    if (visited.find(sourceGraph.get()) == visited.end() && !sourceGraph->_parents.empty())
    {
        // iterate through all of our parents to copy them
        for (const auto& sourceParent : sourceGraph->_parents)
        {
            // If we visited this Profile already...
            auto kv{ visited.find(sourceParent.get()) };
            if (kv != visited.end())
            {
                // add this Profile's clone as a parent
                cloneGraph->InsertParent(kv->second);
            }
            else
            {
                // We have not visited this Profile yet,
                // copy contents of sourceParent to clone
                winrt::com_ptr<Profile> clone{ CopySettings(sourceParent) };

                // add the new copy to the cloneGraph
                cloneGraph->InsertParent(clone);

                // copy the sub-graph at "clone"
                CloneInheritanceGraph(sourceParent, clone, visited);

                // mark clone as "visited"
                // save it to the map in case somebody else references it
                visited[sourceParent.get()] = clone;
            }
        }
    }

    // we have no more to explore down this path.
    return cloneGraph;
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
    stub[JsonKey(GuidKey)] = winrt::to_string(Utils::GuidToString(Guid()));

    stub[JsonKey(NameKey)] = winrt::to_string(Name());

    const auto source{ Source() };
    if (!source.empty())
    {
        stub[JsonKey(SourceKey)] = winrt::to_string(source);
    }

    stub[JsonKey(HiddenKey)] = Hidden();

    return stub;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a Profile object.
// Return Value:
// - a new Profile instance created from the values in `json`
winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile> Profile::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<Profile>();
    result->LayerJson(json);
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
    // First, check that GUIDs match. This is easy. If they don't match, they
    // should _definitely_ not layer.
    const auto otherGuid{ JsonUtils::GetValueForKey<std::optional<winrt::guid>>(json, GuidKey) };
    const auto otherSource{ JsonUtils::GetValueForKey<std::optional<winrt::hstring>>(json, SourceKey) };
    if (otherGuid)
    {
        if (otherGuid.value() != Guid())
        {
            return false;
        }
    }
    else
    {
        // If the other json object didn't have a GUID,
        // check if we auto-generate the same guid using the name and source.
        const auto otherName{ JsonUtils::GetValueForKey<std::optional<winrt::hstring>>(json, NameKey) };
        if (Guid() != _GenerateGuidForProfile(otherName ? *otherName : L"Default", otherSource ? *otherSource : L""))
        {
            return false;
        }
    }

    // For profiles with a `source`, also check the `source` property.
    bool sourceMatches = false;
    const auto mySource{ Source() };
    if (!mySource.empty())
    {
        if (otherSource.has_value())
        {
            // If we have a source and the other has a source, compare them!
            sourceMatches = *otherSource == mySource;
        }
        else
        {
            // Special case the legacy dynamic profiles here. In this case,
            // `this` is a dynamic profile with a source, and our _source is one
            // of the legacy DPG namespaces. We're looking to see if the other
            // json object has the same guid, but _no_ "source"
            if (mySource == WslGeneratorNamespace ||
                mySource == AzureGeneratorNamespace ||
                mySource == PowershellCoreGeneratorNamespace)
            {
                sourceMatches = true;
            }
        }
    }
    else
    {
        // We do not have a source. The only way we match is if source is unset or set to "".
        sourceMatches = (!otherSource.has_value() || otherSource.value() == L"");
    }

    return sourceMatches;
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
    JsonUtils::GetValueForKey(json, NameKey, _Name);
    JsonUtils::GetValueForKey(json, GuidKey, _Guid);
    JsonUtils::GetValueForKey(json, HiddenKey, _Hidden);
    JsonUtils::GetValueForKey(json, SourceKey, _Source);

    // Core Settings
    JsonUtils::GetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::GetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::GetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::GetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::GetValueForKey(json, ColorSchemeKey, _ColorSchemeName);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::GetValueForKey(json, HistorySizeKey, _HistorySize);
    JsonUtils::GetValueForKey(json, SnapOnInputKey, _SnapOnInput);
    JsonUtils::GetValueForKey(json, AltGrAliasingKey, _AltGrAliasing);
    JsonUtils::GetValueForKey(json, CursorHeightKey, _CursorHeight);
    JsonUtils::GetValueForKey(json, CursorShapeKey, _CursorShape);
    JsonUtils::GetValueForKey(json, TabTitleKey, _TabTitle);

    // Control Settings
    JsonUtils::GetValueForKey(json, FontWeightKey, _FontWeight);
    JsonUtils::GetValueForKey(json, ConnectionTypeKey, _ConnectionType);
    JsonUtils::GetValueForKey(json, CommandlineKey, _Commandline);
    JsonUtils::GetValueForKey(json, FontFaceKey, _FontFace);
    JsonUtils::GetValueForKey(json, FontSizeKey, _FontSize);
    JsonUtils::GetValueForKey(json, AcrylicTransparencyKey, _AcrylicOpacity);
    JsonUtils::GetValueForKey(json, UseAcrylicKey, _UseAcrylic);
    JsonUtils::GetValueForKey(json, SuppressApplicationTitleKey, _SuppressApplicationTitle);
    JsonUtils::GetValueForKey(json, CloseOnExitKey, _CloseOnExit);

    // Padding was never specified as an integer, but it was a common working mistake.
    // Allow it to be permissive.
    JsonUtils::GetValueForKey(json, PaddingKey, _Padding, JsonUtils::OptionalConverter<hstring, JsonUtils::PermissiveStringConverter<std::wstring>>{});

    JsonUtils::GetValueForKey(json, ScrollbarStateKey, _ScrollState);

    JsonUtils::GetValueForKey(json, StartingDirectoryKey, _StartingDirectory);

    JsonUtils::GetValueForKey(json, IconKey, _Icon);
    JsonUtils::GetValueForKey(json, BackgroundImageKey, _BackgroundImagePath);
    JsonUtils::GetValueForKey(json, BackgroundImageOpacityKey, _BackgroundImageOpacity);
    JsonUtils::GetValueForKey(json, BackgroundImageStretchModeKey, _BackgroundImageStretchMode);
    JsonUtils::GetValueForKey(json, BackgroundImageAlignmentKey, _BackgroundImageAlignment);
    JsonUtils::GetValueForKey(json, RetroTerminalEffectKey, _RetroTerminalEffect);
    JsonUtils::GetValueForKey(json, AntialiasingModeKey, _AntialiasingMode);
    JsonUtils::GetValueForKey(json, TabColorKey, _TabColor);
    JsonUtils::GetValueForKey(json, BellStyleKey, _BellStyle);
    JsonUtils::GetValueForKey(json, PixelShaderPathKey, _PixelShaderPath);
}

// Method Description:
// - Either Returns this profile's background image path, if one is set, expanding
// - Returns this profile's background image path, if one is set, expanding
//   any environment variables in the path, if there are any.
// - Or if "DesktopWallpaper" is set, then gets the path to the desktops wallpaper.
// Return Value:
// - This profile's expanded background image path / desktops's wallpaper path /the empty string.
winrt::hstring Profile::ExpandedBackgroundImagePath() const
{
    const auto path{ BackgroundImagePath() };
    if (path.empty())
    {
        return path;
    }
    // checks if the user would like to copy their desktop wallpaper
    // if so, replaces the path with the desktop wallpaper's path
    else if (path == DesktopWallpaperEnum)
    {
        WCHAR desktopWallpaper[MAX_PATH];

        // "The returned string will not exceed MAX_PATH characters" as of 2020
        if (SystemParametersInfo(SPI_GETDESKWALLPAPER, MAX_PATH, desktopWallpaper, SPIF_UPDATEINIFILE))
        {
            return winrt::hstring{ (desktopWallpaper) };
        }
        else
        {
            return winrt::hstring{ L"" };
        }
    }
    else
    {
        return winrt::hstring{ wil::ExpandEnvironmentStringsW<std::wstring>(path.c_str()) };
    }
}

winrt::hstring Profile::EvaluatedStartingDirectory() const
{
    auto path{ StartingDirectory() };
    if (!path.empty())
    {
        return winrt::hstring{ Profile::EvaluateStartingDirectory(path.c_str()) };
    }
    // treated as "inherit directory from parent process"
    return path;
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
winrt::guid Profile::_GenerateGuidForProfile(const hstring& name, const hstring& source) noexcept
{
    // If we have a _source, then we can from a dynamic profile generator. Use
    // our source to build the namespace guid, instead of using the default GUID.

    const GUID namespaceGuid = !source.empty() ?
                                   Utils::CreateV5Uuid(RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(source))) :
                                   RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID;

    // Always use the name to generate the temporary GUID. That way, across
    // reloads, we'll generate the same static GUID.
    return { Utils::CreateV5Uuid(namespaceGuid, gsl::as_bytes(gsl::make_span(name))) };
}

// Function Description:
// - Parses the given JSON object to get its GUID. If the json object does not
//   have a `guid` set, we'll generate one, using the `name` field.
// Arguments:
// - json: the JSON object to get a GUID from, or generate a unique GUID for
//   (given the `name`)
// Return Value:
// - The json's `guid`, or a guid synthesized for it.
winrt::guid Profile::GetGuidOrGenerateForJson(const Json::Value& json) noexcept
{
    if (const auto guid{ JsonUtils::GetValueForKey<std::optional<GUID>>(json, GuidKey) })
    {
        return { guid.value() };
    }

    const auto name{ JsonUtils::GetValueForKey<hstring>(json, NameKey) };
    const auto source{ JsonUtils::GetValueForKey<hstring>(json, SourceKey) };

    return Profile::_GenerateGuidForProfile(name, source);
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value Profile::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    // Profile-specific Settings
    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, GuidKey, _Guid);
    JsonUtils::SetValueForKey(json, HiddenKey, _Hidden);
    JsonUtils::SetValueForKey(json, SourceKey, _Source);

    // Core Settings
    JsonUtils::SetValueForKey(json, ForegroundKey, _Foreground);
    JsonUtils::SetValueForKey(json, BackgroundKey, _Background);
    JsonUtils::SetValueForKey(json, SelectionBackgroundKey, _SelectionBackground);
    JsonUtils::SetValueForKey(json, CursorColorKey, _CursorColor);
    JsonUtils::SetValueForKey(json, ColorSchemeKey, _ColorSchemeName);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::SetValueForKey(json, HistorySizeKey, _HistorySize);
    JsonUtils::SetValueForKey(json, SnapOnInputKey, _SnapOnInput);
    JsonUtils::SetValueForKey(json, AltGrAliasingKey, _AltGrAliasing);
    JsonUtils::SetValueForKey(json, CursorHeightKey, _CursorHeight);
    JsonUtils::SetValueForKey(json, CursorShapeKey, _CursorShape);
    JsonUtils::SetValueForKey(json, TabTitleKey, _TabTitle);

    // Control Settings
    JsonUtils::SetValueForKey(json, FontWeightKey, _FontWeight);
    JsonUtils::SetValueForKey(json, ConnectionTypeKey, _ConnectionType);
    JsonUtils::SetValueForKey(json, CommandlineKey, _Commandline);
    JsonUtils::SetValueForKey(json, FontFaceKey, _FontFace);
    JsonUtils::SetValueForKey(json, FontSizeKey, _FontSize);
    JsonUtils::SetValueForKey(json, AcrylicTransparencyKey, _AcrylicOpacity);
    JsonUtils::SetValueForKey(json, UseAcrylicKey, _UseAcrylic);
    JsonUtils::SetValueForKey(json, SuppressApplicationTitleKey, _SuppressApplicationTitle);
    JsonUtils::SetValueForKey(json, CloseOnExitKey, _CloseOnExit);

    // PermissiveStringConverter is unnecessary for serialization
    JsonUtils::SetValueForKey(json, PaddingKey, _Padding);

    JsonUtils::SetValueForKey(json, ScrollbarStateKey, _ScrollState);
    JsonUtils::SetValueForKey(json, StartingDirectoryKey, _StartingDirectory);
    JsonUtils::SetValueForKey(json, IconKey, _Icon);
    JsonUtils::SetValueForKey(json, BackgroundImageKey, _BackgroundImagePath);
    JsonUtils::SetValueForKey(json, BackgroundImageOpacityKey, _BackgroundImageOpacity);
    JsonUtils::SetValueForKey(json, BackgroundImageStretchModeKey, _BackgroundImageStretchMode);
    JsonUtils::SetValueForKey(json, BackgroundImageAlignmentKey, _BackgroundImageAlignment);
    JsonUtils::SetValueForKey(json, RetroTerminalEffectKey, _RetroTerminalEffect);
    JsonUtils::SetValueForKey(json, AntialiasingModeKey, _AntialiasingMode);
    JsonUtils::SetValueForKey(json, TabColorKey, _TabColor);
    JsonUtils::SetValueForKey(json, BellStyleKey, _BellStyle);
    JsonUtils::SetValueForKey(json, PixelShaderPathKey, _PixelShaderPath);

    return json;
}
