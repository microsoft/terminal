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

#include <shellapi.h>

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
static constexpr std::string_view IconKey{ "icon" };

static constexpr std::string_view FontInfoKey{ "font" };
static constexpr std::string_view PaddingKey{ "padding" };
static constexpr std::string_view TabColorKey{ "tabColor" };
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
        unfocusedAppearance->AddLeastImportantParent(parentCom);

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
    profile->_TabColor = _TabColor;
    profile->_Padding = _Padding;
    profile->_Icon = _Icon;

    profile->_Origin = _Origin;
    profile->_FontInfo = *fontInfo;
    profile->_DefaultAppearance = *defaultAppearance;

#define PROFILE_SETTINGS_COPY(type, name, jsonKey, ...) \
    profile->_##name = _##name;
    MTSM_PROFILE_SETTINGS(PROFILE_SETTINGS_COPY)
#undef PROFILE_SETTINGS_COPY

    if (_UnfocusedAppearance)
    {
        Model::AppearanceConfig unfocused{ nullptr };
        if (*_UnfocusedAppearance)
        {
            const auto appearance = AppearanceConfig::CopyAppearance(winrt::get_self<AppearanceConfig>(*_UnfocusedAppearance), weakProfile);
            appearance->AddLeastImportantParent(defaultAppearance);
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
    JsonUtils::GetValueForKey(json, IconKey, _Icon);

    // Padding was never specified as an integer, but it was a common working mistake.
    // Allow it to be permissive.
    JsonUtils::GetValueForKey(json, PaddingKey, _Padding, JsonUtils::OptionalConverter<hstring, JsonUtils::PermissiveStringConverter<std::wstring>>{});

    JsonUtils::GetValueForKey(json, TabColorKey, _TabColor);

#define PROFILE_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(json, jsonKey, _##name);

    MTSM_PROFILE_SETTINGS(PROFILE_SETTINGS_LAYER_JSON)
#undef PROFILE_SETTINGS_LAYER_JSON

    if (json.isMember(JsonKey(UnfocusedAppearanceKey)))
    {
        auto unfocusedAppearance{ winrt::make_self<implementation::AppearanceConfig>(weak_ref<Model::Profile>(*this)) };

        // If an unfocused appearance is defined in this profile, any undefined parameters are
        // taken from this profile's default appearance, so add it as a parent
        com_ptr<AppearanceConfig> parentCom;
        parentCom.copy_from(defaultAppearanceImpl);
        unfocusedAppearance->AddLeastImportantParent(parentCom);

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
                defaultAppearanceImpl->AddLeastImportantParent(parentDefaultAppearanceImpl);
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
                fontInfoImpl->AddLeastImportantParent(parentFontInfoImpl);
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

    const auto namespaceGuid = !source.empty() ?
                                   Utils::CreateV5Uuid(RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID, std::as_bytes(std::span{ source })) :
                                   RUNTIME_GENERATED_PROFILE_NAMESPACE_GUID;

    // Always use the name to generate the temporary GUID. That way, across
    // reloads, we'll generate the same static GUID.
    return { Utils::CreateV5Uuid(namespaceGuid, std::as_bytes(std::span{ name })) };
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
    auto json{ winrt::get_self<implementation::AppearanceConfig>(_DefaultAppearance)->ToJson() };

    // GH #9962:
    //   If the settings.json was missing, when we load the dynamic profiles, they are completely empty.
    //   This caused us to serialize empty profiles "{}" on accident.
    const auto writeBasicSettings{ !Source().empty() };

    // Profile-specific Settings
    JsonUtils::SetValueForKey(json, NameKey, writeBasicSettings ? Name() : _Name);
    JsonUtils::SetValueForKey(json, GuidKey, writeBasicSettings ? Guid() : _Guid);
    JsonUtils::SetValueForKey(json, HiddenKey, writeBasicSettings ? Hidden() : _Hidden);
    JsonUtils::SetValueForKey(json, SourceKey, writeBasicSettings ? Source() : _Source);

    // Recall: Icon isn't actually a setting in the MTSM_PROFILE_SETTINGS. We
    // defined it manually in Profile, so make sure we only serialize the Icon
    // if the user actually changed it here.
    JsonUtils::SetValueForKey(json, IconKey, (writeBasicSettings && HasIcon()) ? Icon() : _Icon);

    // PermissiveStringConverter is unnecessary for serialization
    JsonUtils::SetValueForKey(json, PaddingKey, _Padding);

    JsonUtils::SetValueForKey(json, TabColorKey, _TabColor);

#define PROFILE_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);

    MTSM_PROFILE_SETTINGS(PROFILE_SETTINGS_TO_JSON)
#undef PROFILE_SETTINGS_TO_JSON

    if (auto fontJSON = winrt::get_self<FontConfig>(_FontInfo)->ToJson(); !fontJSON.empty())
    {
        json[JsonKey(FontInfoKey)] = std::move(fontJSON);
    }

    if (_UnfocusedAppearance)
    {
        json[JsonKey(UnfocusedAppearanceKey)] = winrt::get_self<AppearanceConfig>(_UnfocusedAppearance.value())->ToJson();
    }

    return json;
}

// This is the implementation for and INHERITABLE_SETTING, but with one addition
// in the setter. We want to make sure to clear out our cached icon, so that we
// can re-evaluate it as it changes in the SUI.
void Profile::Icon(const winrt::hstring& value)
{
    _evaluatedIcon = std::nullopt;
    _Icon = value;
}
winrt::hstring Profile::Icon() const
{
    const auto val{ _getIconImpl() };
    return val ? *val : hstring{ L"\uE756" };
}

winrt::hstring Profile::EvaluatedIcon()
{
    // We cache the result here, so we don't search the path for the exe every time.
    if (!_evaluatedIcon.has_value())
    {
        _evaluatedIcon = _evaluateIcon();
    }
    return *_evaluatedIcon;
}

winrt::hstring Profile::_evaluateIcon() const
{
    // If the profile has an icon, return it.
    if (!Icon().empty())
    {
        return Icon();
    }

    // Otherwise, use NormalizeCommandLine to find the actual exe name. This
    // will actually search for the exe, including spaces, in the same way that
    // CreateProcess does.
    std::wstring cmdline{ NormalizeCommandLine(Commandline().c_str()) };
    // NormalizeCommandLine will return a string with embedded nulls after each
    // arg. We just want the first one.
    return winrt::hstring{ cmdline.c_str() };
}

// Given a commandLine like the following:
// * "C:\WINDOWS\System32\cmd.exe"
// * "pwsh -WorkingDirectory ~"
// * "C:\Program Files\PowerShell\7\pwsh.exe"
// * "C:\Program Files\PowerShell\7\pwsh.exe -WorkingDirectory ~"
//
// This function returns:
// * "C:\Windows\System32\cmd.exe"
// * "C:\Program Files\PowerShell\7\pwsh.exe\0-WorkingDirectory\0~"
// * "C:\Program Files\PowerShell\7\pwsh.exe"
// * "C:\Program Files\PowerShell\7\pwsh.exe\0-WorkingDirectory\0~"
//
// The resulting strings are then used for comparisons in _getProfileForCommandLine().
// For instance a resulting string of
//   "C:\Program Files\PowerShell\7\pwsh.exe"
// is considered a compatible profile with
//   "C:\Program Files\PowerShell\7\pwsh.exe -WorkingDirectory ~"
// as it shares the same (normalized) prefix.
std::wstring Profile::NormalizeCommandLine(LPCWSTR commandLine)
{
    // Turn "%SystemRoot%\System32\cmd.exe" into "C:\WINDOWS\System32\cmd.exe".
    // We do this early, as environment variables might occur anywhere in the commandLine.
    std::wstring normalized;
    THROW_IF_FAILED(wil::ExpandEnvironmentStringsW(commandLine, normalized));

    // One of the most important things this function does is to strip quotes.
    // That way the commandLine "foo.exe -bar" and "\"foo.exe\" \"-bar\"" appear identical.
    // We'll abuse CommandLineToArgvW for that as it's close to what CreateProcessW uses.
    auto argc = 0;
    wil::unique_hlocal_ptr<PWSTR[]> argv{ CommandLineToArgvW(normalized.c_str(), &argc) };
    THROW_LAST_ERROR_IF(!argc);

    // The index of the first argument in argv for our executable in argv[0].
    // Given {"C:\Program Files\PowerShell\7\pwsh.exe", "-WorkingDirectory", "~"} this will be 1.
    auto startOfArguments = 1;

    // The given commandLine should start with an executable name or path.
    // For instance given the following argv arrays:
    // * {"C:\WINDOWS\System32\cmd.exe"}
    // * {"pwsh", "-WorkingDirectory", "~"}
    // * {"C:\Program", "Files\PowerShell\7\pwsh.exe"}
    //               ^^^^
    //   Notice how there used to be a space in the path, which was split by ExpandEnvironmentStringsW().
    //   CreateProcessW() supports such atrocities, so we got to do the same.
    // * {"C:\Program Files\PowerShell\7\pwsh.exe", "-WorkingDirectory", "~"}
    //
    // This loop tries to resolve relative paths, as well as executable names in %PATH%
    // into absolute paths and normalizes them. The results for the above would be:
    // * "C:\Windows\System32\cmd.exe"
    // * "C:\Program Files\PowerShell\7\pwsh.exe"
    // * "C:\Program Files\PowerShell\7\pwsh.exe"
    // * "C:\Program Files\PowerShell\7\pwsh.exe"
    for (;;)
    {
        // CreateProcessW uses RtlGetExePath to get the lpPath for SearchPathW.
        // The difference between the behavior of SearchPathW if lpPath is nullptr and what RtlGetExePath returns
        // seems to be mostly whether SafeProcessSearchMode is respected and the support for relative paths.
        // Windows Terminal makes the use of relative paths rather impractical which is why we simply dropped the call to RtlGetExePath.
        const auto status = wil::SearchPathW(nullptr, argv[0], L".exe", normalized);

        if (status == S_OK)
        {
            const auto attributes = GetFileAttributesW(normalized.c_str());

            if (attributes != INVALID_FILE_ATTRIBUTES && WI_IsFlagClear(attributes, FILE_ATTRIBUTE_DIRECTORY))
            {
                std::filesystem::path path{ std::move(normalized) };

                // canonical() will resolve symlinks, etc. for us.
                {
                    std::error_code ec;
                    auto canonicalPath = std::filesystem::canonical(path, ec);
                    if (!ec)
                    {
                        path = std::move(canonicalPath);
                    }
                }

                // std::filesystem::path has no way to extract the internal path.
                // So about that.... I own you, computer. Give me that path.
                normalized = std::move(const_cast<std::wstring&>(path.native()));
                break;
            }
        }
        // All other error types aren't handled at the moment.
        else if (status != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            break;
        }
        // If the file path couldn't be found by SearchPathW this could be the result of us being given a commandLine
        // like "C:\foo bar\baz.exe -arg" which is resolved to the argv array {"C:\foo", "bar\baz.exe", "-arg"},
        // or we were erroneously given a directory to execute (e.g. someone ran `wt .`).
        // Just like CreateProcessW() we thus try to concatenate arguments until we successfully resolve a valid path.
        // Of course we can only do that if we have at least 2 remaining arguments in argv.
        if ((argc - startOfArguments) < 2)
        {
            break;
        }

        // As described in the comment right above, we concatenate arguments in an attempt to resolve a valid path.
        // The code below turns argv from {"C:\foo", "bar\baz.exe", "-arg"} into {"C:\foo bar\baz.exe", "-arg"}.
        // The code abuses the fact that CommandLineToArgvW allocates all arguments back-to-back on the heap separated by '\0'.
        argv[startOfArguments][-1] = L' ';
        ++startOfArguments;
    }

    // We've (hopefully) finished resolving the path to the executable.
    // We're now going to append all remaining arguments to the resulting string.
    // If argv is {"C:\Program Files\PowerShell\7\pwsh.exe", "-WorkingDirectory", "~"},
    // then we'll get "C:\Program Files\PowerShell\7\pwsh.exe\0-WorkingDirectory\0~"
    if (startOfArguments < argc)
    {
        // normalized contains a canonical form of argv[0] at this point.
        // -1 allows us to include the \0 between argv[0] and argv[1] in the call to append().
        const auto beg = argv[startOfArguments] - 1;
        const auto lastArg = argv[argc - 1];
        const auto end = lastArg + wcslen(lastArg);
        normalized.append(beg, end);
    }

    return normalized;
}
