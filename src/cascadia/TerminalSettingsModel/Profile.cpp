// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profile.h"
#include "JsonUtils.h"
#include "../../types/inc/Utils.hpp"
#include <DefaultSettings.h>

#include "LegacyProfileGeneratorNamespaces.h"
#include "TerminalSettingsSerializationHelpers.h"
#include "AppearanceConfig.h"

#include "Profile.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

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
static constexpr std::string_view AntialiasingModeKey{ "antialiasingMode" };
static constexpr std::string_view TabColorKey{ "tabColor" };
static constexpr std::string_view BellStyleKey{ "bellStyle" };
static constexpr std::string_view UnfocusedAppearanceKey{ "unfocusedAppearance" };

static constexpr std::wstring_view DesktopWallpaperEnum{ L"desktopWallpaper" };

Profile::Profile()
{
}

Profile::Profile(guid guid) :
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
    if (_UnfocusedAppearance)
    {
        _UnfocusedAppearance = std::nullopt;
    }
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
    profile->_AntialiasingMode = source->_AntialiasingMode;
    profile->_ForceFullRepaintRendering = source->_ForceFullRepaintRendering;
    profile->_SoftwareRendering = source->_SoftwareRendering;
    profile->_HistorySize = source->_HistorySize;
    profile->_SnapOnInput = source->_SnapOnInput;
    profile->_AltGrAliasing = source->_AltGrAliasing;
    profile->_BellStyle = source->_BellStyle;
    profile->_ConnectionType = source->_ConnectionType;
    profile->_Origin = source->_Origin;

    // Copy over the appearance
    const auto weakRefToProfile = weak_ref<Model::Profile>(*profile);
    winrt::com_ptr<AppearanceConfig> sourceDefaultAppearanceImpl;
    sourceDefaultAppearanceImpl.copy_from(winrt::get_self<AppearanceConfig>(source->_DefaultAppearance));
    auto copiedDefaultAppearance = AppearanceConfig::CopyAppearance(sourceDefaultAppearanceImpl, weakRefToProfile);
    profile->_DefaultAppearance = *copiedDefaultAppearance;

    if (source->_UnfocusedAppearance.has_value())
    {
        Model::AppearanceConfig unfocused{ nullptr };
        if (source->_UnfocusedAppearance.value() != nullptr)
        {
            // Copy over the unfocused appearance
            winrt::com_ptr<AppearanceConfig> sourceUnfocusedAppearanceImpl;
            sourceUnfocusedAppearanceImpl.copy_from(winrt::get_self<AppearanceConfig>(source->_UnfocusedAppearance.value()));
            auto copiedUnfocusedAppearance = AppearanceConfig::CopyAppearance(sourceUnfocusedAppearanceImpl, weakRefToProfile);

            // Make sure to add the default appearance as a parent
            copiedUnfocusedAppearance->InsertParent(copiedDefaultAppearance);
            unfocused = *copiedUnfocusedAppearance;
        }
        profile->_UnfocusedAppearance = unfocused;
    }

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
                InsertParentHelper(cloneGraph, kv->second);
            }
            else
            {
                // We have not visited this Profile yet,
                // copy contents of sourceParent to clone
                winrt::com_ptr<Profile> clone{ CopySettings(sourceParent) };

                // add the new copy to the cloneGraph
                InsertParentHelper(cloneGraph, clone);

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
// - Inserts a parent profile into a child profile, at the specified index if one was provided
// - Makes sure to call _FinalizeInheritance after inserting the parent
// Arguments:
// - child: the child profile to insert the parent into
// - parent: the parent profile to insert into the child
// - index: an optional index value to insert the parent into
void Profile::InsertParentHelper(winrt::com_ptr<Profile> child, winrt::com_ptr<Profile> parent, std::optional<size_t> index)
{
    if (index)
    {
        child->InsertParent(index.value(), parent);
    }
    else
    {
        child->InsertParent(parent);
    }
    child->_FinalizeInheritance();
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
    // Appearance Settings
    auto defaultAppearanceImpl = winrt::get_self<implementation::AppearanceConfig>(_DefaultAppearance);
    defaultAppearanceImpl->LayerJson(json);

    // Profile-specific Settings
    JsonUtils::GetValueForKey(json, NameKey, _Name);
    JsonUtils::GetValueForKey(json, GuidKey, _Guid);
    JsonUtils::GetValueForKey(json, HiddenKey, _Hidden);
    JsonUtils::GetValueForKey(json, SourceKey, _Source);

    // TODO:MSFT:20642297 - Use a sentinel value (-1) for "Infinite scrollback"
    JsonUtils::GetValueForKey(json, HistorySizeKey, _HistorySize);
    JsonUtils::GetValueForKey(json, SnapOnInputKey, _SnapOnInput);
    JsonUtils::GetValueForKey(json, AltGrAliasingKey, _AltGrAliasing);
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
}

winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig Profile::DefaultAppearance()
{
    return _DefaultAppearance;
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
    return std::wstring(evaluatedPath.get(), numCharsInput);
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
    JsonUtils::SetValueForKey(json, AntialiasingModeKey, _AntialiasingMode);
    JsonUtils::SetValueForKey(json, TabColorKey, _TabColor);
    JsonUtils::SetValueForKey(json, BellStyleKey, _BellStyle);

    if (_UnfocusedAppearance)
    {
        json[JsonKey(UnfocusedAppearanceKey)] = winrt::get_self<AppearanceConfig>(_UnfocusedAppearance.value())->ToJson();
    }

    return json;
}
