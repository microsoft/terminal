// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"

#include "TerminalSettingsSerializationHelpers.h"
#include "AppearanceConfig.h"
#include "FontConfig.h"

#include "Profile.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

static constexpr std::string_view UpdatesKey{ "updates" };
static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view GuidKey{ "guid" };
static constexpr std::string_view SourceKey{ "source" };
static constexpr std::string_view HiddenKey{ "hidden" };

static constexpr std::string_view TabTitleKey{ "tabTitle" };
static constexpr std::string_view SuppressApplicationTitleKey{ "suppressApplicationTitle" };
static constexpr std::string_view HistorySizeKey{ "historySize" };
static constexpr std::string_view SnapOnInputKey{ "snapOnInput" };
static constexpr std::string_view AltGrAliasingKey{ "altGrAliasing" };

static constexpr std::string_view ConnectionTypeKey{ "connectionType" };
static constexpr std::string_view CommandlineKey{ "commandline" };
static constexpr std::string_view FontInfoKey{ "font" };
static constexpr std::string_view UseAcrylicKey{ "useAcrylic" };
static constexpr std::string_view ScrollbarStateKey{ "scrollbarState" };
static constexpr std::string_view CloseOnExitKey{ "closeOnExit" };
static constexpr std::string_view PaddingKey{ "padding" };
static constexpr std::string_view StartingDirectoryKey{ "startingDirectory" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view AntialiasingModeKey{ "antialiasingMode" };
static constexpr std::string_view TabColorKey{ "tabColor" };
static constexpr std::string_view BellStyleKey{ "bellStyle" };
static constexpr std::string_view UnfocusedAppearanceKey{ "unfocusedAppearance" };

Profile::Profile(guid guid) noexcept :
    _Guid(guid)
{
}

void Profile::CreateUnfocusedAppearance()
{
    if (!_UnfocusedAppearance)
    {
        auto unfocusedAppearance{ winrt::make_self<implementation::AppearanceConfig>(weak_ref<Model::Profile>(*this)) };

        // If an unfocused appearance is defined in this profile, any undefined parameters are
        // taken from this profile's default appearance, so add it as a parent
        com_ptr<AppearanceConfig> parentCom;
        parentCom.copy_from(winrt::get_self<implementation::AppearanceConfig>(_DefaultAppearance));
        unfocusedAppearance->InsertParent(parentCom);

        _UnfocusedAppearance = *unfocusedAppearance;
    }
}

void Profile::DeleteUnfocusedAppearance()
{
    _UnfocusedAppearance = std::nullopt;
}

// See CopyInheritanceGraph (singular) for more information.
// This does the same, but runs it on a list of graph nodes and clones each sub-graph.
void Profile::CopyInheritanceGraphs(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited, const std::vector<winrt::com_ptr<Profile>>& source, std::vector<winrt::com_ptr<Profile>>& target)
{
    for (const auto& sourceProfile : source)
    {
        target.emplace_back(sourceProfile->CopyInheritanceGraph(visited));
    }
}

// A profile and its IInheritable parents basically behave like a directed acyclic graph (DAG).
// Cloning a DAG requires us to prevent the duplication of already cloned nodes (or profiles).
// This is where "visited" comes into play: It contains previously cloned sub-graphs of profiles and "interns" them.
winrt::com_ptr<Profile>& Profile::CopyInheritanceGraph(std::unordered_map<const Profile*, winrt::com_ptr<Profile>>& visited) const
{
    // The operator[] is usually considered to suck, because it implicitly creates entries
    // in maps/sets if the entry doesn't exist yet, which is often an unwanted behavior.
    // But in this case it's just perfect. We want to return a reference to the profile if it's
    // been created before and create a cloned profile if it doesn't. With the operator[]
    // we can just assign the returned reference allowing us to write some lean code.
    auto& clone = visited[this];

    if (!clone)
    {
        clone = CopySettings();
        CopyInheritanceGraphs(visited, _parents, clone->_parents);
        clone->_FinalizeInheritance();
    }

    return clone;
}

winrt::com_ptr<Profile> Profile::CopySettings() const
{
    const auto profile = winrt::make_self<Profile>();
    const auto weakProfile = winrt::make_weak<Model::Profile>(*profile);
    const auto fontInfo = FontConfig::CopyFontInfo(winrt::get_self<FontConfig>(_FontInfo), weakProfile);
    const auto defaultAppearance = AppearanceConfig::CopyAppearance(winrt::get_self<AppearanceConfig>(_DefaultAppearance), weakProfile);

    profile->_Deleted = _Deleted;
    profile->_Updates = _Updates;
    profile->_Guid = _Guid;
    profile->_Name = _Name;
    profile->_Source = _Source;
    profile->_Hidden = _Hidden;
    profile->_Icon = _Icon;
    profile->_CloseOnExit = _CloseOnExit;
    profile->_TabTitle = _TabTitle;
    profile->_TabColor = _TabColor;
    profile->_SuppressApplicationTitle = _SuppressApplicationTitle;
    profile->_UseAcrylic = _UseAcrylic;
    profile->_ScrollState = _ScrollState;
    profile->_Padding = _Padding;
    profile->_Commandline = _Commandline;
    profile->_StartingDirectory = _StartingDirectory;
    profile->_AntialiasingMode = _AntialiasingMode;
    profile->_ForceFullRepaintRendering = _ForceFullRepaintRendering;
    profile->_SoftwareRendering = _SoftwareRendering;
    profile->_HistorySize = _HistorySize;
    profile->_SnapOnInput = _SnapOnInput;
    profile->_AltGrAliasing = _AltGrAliasing;
    profile->_BellStyle = _BellStyle;
    profile->_ConnectionType = _ConnectionType;
    profile->_Origin = _Origin;
    profile->_FontInfo = *fontInfo;
    profile->_DefaultAppearance = *defaultAppearance;

    if (_UnfocusedAppearance)
    {
        Model::AppearanceConfig unfocused{ nullptr };
        if (*_UnfocusedAppearance)
        {
            const auto appearance = AppearanceConfig::CopyAppearance(winrt::get_self<AppearanceConfig>(*_UnfocusedAppearance), weakProfile);
            appearance->InsertParent(defaultAppearance);
            unfocused = *appearance;
        }
        profile->_UnfocusedAppearance = unfocused;
    }

    return profile;
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
    // Appearance Settings
    auto defaultAppearanceImpl = winrt::get_self<implementation::AppearanceConfig>(_DefaultAppearance);
    defaultAppearanceImpl->LayerJson(json);

    // Font Settings
    auto fontInfoImpl = winrt::get_self<implementation::FontConfig>(_FontInfo);
    fontInfoImpl->LayerJson(json);

    // Profile-specific Settings
    JsonUtils::GetValueForKey(json, NameKey, _Name);
    JsonUtils::GetValueForKey(json, UpdatesKey, _Updates);
    JsonUtils::GetValueForKey(json, GuidKey, _Guid);
    JsonUtils::GetValueForKey(json, HiddenKey, _Hidden);
    JsonUtils::GetValueForKey(json, SourceKey, _Source);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::GetValueForKey(json, HistorySizeKey, _HistorySize);
    JsonUtils::GetValueForKey(json, SnapOnInputKey, _SnapOnInput);
    JsonUtils::GetValueForKey(json, AltGrAliasingKey, _AltGrAliasing);
    JsonUtils::GetValueForKey(json, TabTitleKey, _TabTitle);

    // Control Settings
    JsonUtils::GetValueForKey(json, ConnectionTypeKey, _ConnectionType);
    JsonUtils::GetValueForKey(json, CommandlineKey, _Commandline);
    JsonUtils::GetValueForKey(json, UseAcrylicKey, _UseAcrylic);
    JsonUtils::GetValueForKey(json, SuppressApplicationTitleKey, _SuppressApplicationTitle);
    JsonUtils::GetValueForKey(json, CloseOnExitKey, _CloseOnExit);

    // Padding was never specified as an integer, but it was a common working mistake.
    // Allow it to be permissive.
    JsonUtils::GetValueForKey(json, PaddingKey, _Padding, JsonUtils::OptionalConverter<hstring, JsonUtils::PermissiveStringConverter<std::wstring>>{});

    JsonUtils::GetValueForKey(json, ScrollbarStateKey, _ScrollState);

    JsonUtils::GetValueForKey(json, StartingDirectoryKey, _StartingDirectory);

    JsonUtils::GetValueForKey(json, IconKey, _Icon);
    JsonUtils::GetValueForKey(json, AntialiasingModeKey, _AntialiasingMode);
    JsonUtils::GetValueForKey(json, TabColorKey, _TabColor);
    JsonUtils::GetValueForKey(json, BellStyleKey, _BellStyle);

    if (json.isMember(JsonKey(UnfocusedAppearanceKey)))
    {
        auto unfocusedAppearance{ winrt::make_self<implementation::AppearanceConfig>(weak_ref<Model::Profile>(*this)) };

        // If an unfocused appearance is defined in this profile, any undefined parameters are
        // taken from this profile's default appearance, so add it as a parent
        com_ptr<AppearanceConfig> parentCom;
        parentCom.copy_from(defaultAppearanceImpl);
        unfocusedAppearance->InsertParent(parentCom);

        unfocusedAppearance->LayerJson(json[JsonKey(UnfocusedAppearanceKey)]);
        _UnfocusedAppearance = *unfocusedAppearance;
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

void Profile::_FinalizeInheritance()
{
    if (auto defaultAppearanceImpl = get_self<AppearanceConfig>(_DefaultAppearance))
    {
        // Clear any existing parents first, we don't want duplicates from any previous
        // calls to this function
        defaultAppearanceImpl->ClearParents();
        for (auto& parent : _parents)
        {
            if (auto parentDefaultAppearanceImpl = parent->_DefaultAppearance.try_as<AppearanceConfig>())
            {
                defaultAppearanceImpl->InsertParent(parentDefaultAppearanceImpl);
            }
        }
    }
    if (auto fontInfoImpl = get_self<FontConfig>(_FontInfo))
    {
        // Clear any existing parents first, we don't want duplicates from any previous
        // calls to this function
        fontInfoImpl->ClearParents();
        for (auto& parent : _parents)
        {
            if (auto parentFontInfoImpl = parent->_FontInfo.try_as<FontConfig>())
            {
                fontInfoImpl->InsertParent(parentFontInfoImpl);
            }
        }
    }
}

winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig Profile::DefaultAppearance()
{
    return _DefaultAppearance;
}

winrt::Microsoft::Terminal::Settings::Model::FontConfig Profile::FontInfo()
{
    return _FontInfo;
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
    // Prior to GH#9541, we'd validate that the user's startingDirectory existed
    // here. If it was invalid, we'd gracefully fall back to %USERPROFILE%.
    //
    // However, that could cause hangs when combined with WSL. When the WSL
    // filesystem is slow to respond, we'll end up waiting indefinitely for
    // their filesystem driver to respond. This can result in the whole terminal
    // becoming unresponsive.
    //
    // If the path is eventually invalid, we'll display warning in the
    // ConptyConnection when the process fails to launch.
    return wil::ExpandEnvironmentStringsW<std::wstring>(directory.c_str());
}

// Function Description:
// - Generates a unique guid for a profile, given the name. For an given name, will always return the same GUID.
// Arguments:
// - name: The name to generate a unique GUID from
// Return Value:
// - a uuidv5 GUID generated from the given name.
winrt::guid Profile::_GenerateGuidForProfile(const std::wstring_view& name, const std::wstring_view& source) noexcept
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

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value Profile::ToJson() const
{
    // Initialize the json with the appearance settings
    Json::Value json{ winrt::get_self<implementation::AppearanceConfig>(_DefaultAppearance)->ToJson() };

    // GH #9962:
    //   If the settings.json was missing, when we load the dynamic profiles, they are completely empty.
    //   This caused us to serialize empty profiles "{}" on accident.
    const bool writeBasicSettings{ !Source().empty() };

    // Profile-specific Settings
    JsonUtils::SetValueForKey(json, NameKey, writeBasicSettings ? Name() : _Name);
    JsonUtils::SetValueForKey(json, GuidKey, writeBasicSettings ? Guid() : _Guid);
    JsonUtils::SetValueForKey(json, HiddenKey, writeBasicSettings ? Hidden() : _Hidden);
    JsonUtils::SetValueForKey(json, SourceKey, writeBasicSettings ? Source() : _Source);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::SetValueForKey(json, HistorySizeKey, _HistorySize);
    JsonUtils::SetValueForKey(json, SnapOnInputKey, _SnapOnInput);
    JsonUtils::SetValueForKey(json, AltGrAliasingKey, _AltGrAliasing);
    JsonUtils::SetValueForKey(json, TabTitleKey, _TabTitle);

    // Control Settings
    JsonUtils::SetValueForKey(json, ConnectionTypeKey, _ConnectionType);
    JsonUtils::SetValueForKey(json, CommandlineKey, _Commandline);
    JsonUtils::SetValueForKey(json, UseAcrylicKey, _UseAcrylic);
    JsonUtils::SetValueForKey(json, SuppressApplicationTitleKey, _SuppressApplicationTitle);
    JsonUtils::SetValueForKey(json, CloseOnExitKey, _CloseOnExit);

    // PermissiveStringConverter is unnecessary for serialization
    JsonUtils::SetValueForKey(json, PaddingKey, _Padding);

    JsonUtils::SetValueForKey(json, ScrollbarStateKey, _ScrollState);
    JsonUtils::SetValueForKey(json, StartingDirectoryKey, _StartingDirectory);
    JsonUtils::SetValueForKey(json, IconKey, _Icon);
    JsonUtils::SetValueForKey(json, AntialiasingModeKey, _AntialiasingMode);
    JsonUtils::SetValueForKey(json, TabColorKey, _TabColor);
    JsonUtils::SetValueForKey(json, BellStyleKey, _BellStyle);

    // Font settings
    const auto fontInfoImpl = winrt::get_self<FontConfig>(_FontInfo);
    if (fontInfoImpl->HasAnyOptionSet())
    {
        json[JsonKey(FontInfoKey)] = winrt::get_self<FontConfig>(_FontInfo)->ToJson();
    }

    if (_UnfocusedAppearance)
    {
        json[JsonKey(UnfocusedAppearanceKey)] = winrt::get_self<AppearanceConfig>(_UnfocusedAppearance.value())->ToJson();
    }

    return json;
}
