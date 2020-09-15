// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"
#include "..\TerminalApp\Utils.h"
#include "JsonUtils.h"
#include <appmodel.h>
#include <shlobj.h>

// defaults.h is a file containing the default json settings in a std::string_view
#include "defaults.h"
#include "defaults-universal.h"
// userDefault.h is like the above, but with a default template for the user's settings.json.
#include "userDefaults.h"
// Both defaults.h and userDefaults.h are generated at build time into the
// "Generated Files" directory.

using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace ::Microsoft::Console;

static constexpr std::wstring_view SettingsFilename{ L"settings.json" };
static constexpr std::wstring_view LegacySettingsFilename{ L"profiles.json" };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };

static constexpr std::wstring_view DefaultsFilename{ L"defaults.json" };

static constexpr std::string_view SchemaKey{ "$schema" };
static constexpr std::string_view ProfilesKey{ "profiles" };
static constexpr std::string_view DefaultSettingsKey{ "defaults" };
static constexpr std::string_view ProfilesListKey{ "list" };
static constexpr std::string_view KeybindingsKey{ "keybindings" };
static constexpr std::string_view SchemesKey{ "schemes" };

static constexpr std::string_view DisabledProfileSourcesKey{ "disabledProfileSources" };

static constexpr std::string_view Utf8Bom{ u8"\uFEFF" };
static constexpr std::string_view SettingsSchemaFragment{ "\n"
                                                          R"(    "$schema": "https://aka.ms/terminal-profiles-schema")" };

static std::tuple<size_t, size_t> _LineAndColumnFromPosition(const std::string_view string, ptrdiff_t position)
{
    size_t line = 1, column = position + 1;
    auto lastNL = string.find_last_of('\n', position);
    if (lastNL != std::string::npos)
    {
        column = (position - lastNL);
        line = std::count(string.cbegin(), string.cbegin() + lastNL + 1, '\n') + 1;
    }

    return { line, column };
}

static void _CatchRethrowSerializationExceptionWithLocationInfo(std::string_view settingsString)
{
    std::string msg;

    try
    {
        throw;
    }
    catch (const JsonUtils::DeserializationError& e)
    {
        static constexpr std::string_view basicHeader{ "* Line {line}, Column {column}\n{message}" };
        static constexpr std::string_view keyedHeader{ "* Line {line}, Column {column} ({key})\n{message}" };

        std::string jsonValueAsString{ "array or object" };
        try
        {
            jsonValueAsString = e.jsonValue.asString();
        }
        catch (...)
        {
            // discard: we're in the middle of error handling
        }

        msg = fmt::format("  Have: \"{}\"\n  Expected: {}", jsonValueAsString, e.expectedType);

        auto [l, c] = _LineAndColumnFromPosition(settingsString, e.jsonValue.getOffsetStart());
        msg = fmt::format((e.key ? keyedHeader : basicHeader),
                          fmt::arg("line", l),
                          fmt::arg("column", c),
                          fmt::arg("key", e.key.value_or("")),
                          fmt::arg("message", msg));
        throw SettingsTypedDeserializationException{ msg };
    }
}

// Method Description:
// - Creates a CascadiaSettings from whatever's saved on disk, or instantiates
//      a new one with the default values. If we're running as a packaged app,
//      it will load the settings from our packaged localappdata. If we're
//      running as an unpackaged application, it will read it from the path
//      we've set under localappdata.
// - Loads both the settings from the defaults.json and the user's settings.json
// - Also runs and dynamic profile generators. If any of those generators create
//   new profiles, we'll write the user settings back to the file, with the new
//   profiles inserted into their list of profiles.
// Return Value:
// - a unique_ptr containing a new CascadiaSettings object.
winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings CascadiaSettings::LoadAll()
{
    try
    {
        auto settings = LoadDefaults();
        auto resultPtr = winrt::get_self<CascadiaSettings>(settings);

        // GH 3588, we need this below to know if the user chose something that wasn't our default.
        // Collect it up here in case it gets modified by any of the other layers between now and when
        // the user's preferences are loaded and layered.
        const auto hardcodedDefaultGuid = resultPtr->GlobalSettings().DefaultProfile();

        std::optional<std::string> fileData = _ReadUserSettings();
        const bool foundFile = fileData.has_value();

        // Make sure the file isn't totally empty. If it is, we'll treat the file
        // like it doesn't exist at all.
        const bool fileHasData = foundFile && !fileData.value().empty();
        bool needToWriteFile = false;
        if (fileHasData)
        {
            resultPtr->_ParseJsonString(fileData.value(), false);
        }

        // Load profiles from dynamic profile generators. _userSettings should be
        // created by now, because we're going to check in there for any generators
        // that should be disabled (if the user had any settings.)
        resultPtr->_LoadDynamicProfiles();

        if (!fileHasData)
        {
            // We didn't find the user settings. We'll need to create a file
            // to use as the user defaults.
            // For now, just parse our user settings template as their user settings.
            auto userSettings{ resultPtr->_ApplyFirstRunChangesToSettingsTemplate(UserSettingsJson) };
            resultPtr->_ParseJsonString(userSettings, false);
            needToWriteFile = true;
        }

        try
        {
            // See microsoft/terminal#2325: find the defaultSettings from the user's
            // settings. Layer those settings upon all the existing profiles we have
            // (defaults and dynamic profiles). We'll also set
            // _userDefaultProfileSettings here. When we LayerJson below to apply the
            // user settings, we'll make sure to use these defaultSettings _before_ any
            // profiles the user might have.
            resultPtr->_ApplyDefaultsFromUserSettings();

            // Apply the user's settings
            resultPtr->LayerJson(resultPtr->_userSettings);
        }
        catch (...)
        {
            _CatchRethrowSerializationExceptionWithLocationInfo(resultPtr->_userSettingsString);
        }

        // After layering the user settings, check if there are any new profiles
        // that need to be inserted into their user settings file.
        needToWriteFile = resultPtr->_AppendDynamicProfilesToUserSettings() || needToWriteFile;

        if (needToWriteFile)
        {
            // For safety's sake, we need to re-parse the JSON document to ensure that
            // all future patches are applied with updated object offsets.
            resultPtr->_ParseJsonString(resultPtr->_userSettingsString, false);
        }

        // Make sure there's a $schema at the top of the file.
        needToWriteFile = resultPtr->_PrependSchemaDirective() || needToWriteFile;

        // TODO:GH#2721 If powershell core is installed, we need to set that to the
        // default profile, but only when the settings file was newly created. We'll
        // re-write the segment of the user settings for "default profile" to have
        // the powershell core GUID instead.

        // If we created the file, or found new dynamic profiles, write the user
        // settings string back to the file.
        if (needToWriteFile)
        {
            // If AppendDynamicProfilesToUserSettings (or the pwsh check above)
            // changed the file, then our local settings JSON is no longer accurate.
            // We should re-parse, but not re-layer
            resultPtr->_ParseJsonString(resultPtr->_userSettingsString, false);

            _WriteSettings(resultPtr->_userSettingsString);
        }

        // If this throws, the app will catch it and use the default settings
        resultPtr->_ValidateSettings();

        // GH 3855 - Gathering Data on custom profiles to inform better defaults
        // Do it after everything else so it won't happen unless validation passed.
        // Also, avoid processing unless someone's listening for measures. The keybindings work, at least,
        // is a lot of computation we can skip if no one cares.
        if (TraceLoggingProviderEnabled(g_hTerminalAppProvider, 0, MICROSOFT_KEYWORD_MEASURES))
        {
            const auto guid = resultPtr->GlobalSettings().DefaultProfile();

            // Compare to the defaults.json one that we set on install.
            // If it's different, log what the user chose.
            if (hardcodedDefaultGuid != guid)
            {
                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "CustomDefaultProfile",
                    TraceLoggingDescription("Event emitted when user has chosen a different default profile than hardcoded one on load/reload"),
                    TraceLoggingGuid(guid, "DefaultProfile", "ID of user-chosen default profile"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
            }

            // If the user had keybinding settings preferences, we want to learn from them to make better defaults
            auto userKeybindings = resultPtr->_userSettings[JsonKey(KeybindingsKey)];
            if (!userKeybindings.empty())
            {
                // If there are custom key bindings, let's understand what they are because maybe the defaults aren't good enough

                // Run it through the object so we can parse it apart and then only serialize the fields we're interested in
                // and avoid extraneous data.
                auto km = winrt::make_self<implementation::KeyMapping>();
                km->LayerJson(userKeybindings);
                auto value = km->ToJson();

                // Reserialize the keybindings
                Json::StreamWriterBuilder wbuilder;
                // Use 4 spaces to indent instead of \t
                wbuilder.settings_["indentation"] = "    ";
                wbuilder.settings_["enableYAMLCompatibility"] = true; // suppress spaces around colons

                const auto keybindingsString = Json::writeString(wbuilder, value);

                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "CustomKeybindings",
                    TraceLoggingDescription("Event emitted when custom keybindings are identified on load/reload"),
                    TraceLoggingUtf8String(keybindingsString.c_str(), "Keybindings", "Keybindings as JSON"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
            }
        }

        return *resultPtr;
    }
    catch (const SettingsException& ex)
    {
        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        settings->_loadError = ex.Error();
        return *settings;
    }
    catch (const SettingsTypedDeserializationException& e)
    {
        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        std::string_view what{ e.what() };
        settings->_deserializationErrorMessage = til::u8u16(what);
        return *settings;
    }
}

// Function Description:
// - Loads a batch of settings curated for the Universal variant of the terminal app
// Arguments:
// - <none>
// Return Value:
// - a unique_ptr to a CascadiaSettings with the connection types and settings for Universal terminal
winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings CascadiaSettings::LoadUniversal()
{
    // We're going to do this ourselves because we want to exclude almost everything
    // from the special Universal-for-developers configuration

    try
    {
        // Create settings and get the universal defaults loaded up.
        auto resultPtr = winrt::make_self<CascadiaSettings>();
        resultPtr->_ParseJsonString(DefaultUniversalJson, true);
        resultPtr->LayerJson(resultPtr->_defaultSettings);

        // Now validate.
        // If this throws, the app will catch it and use the default settings
        resultPtr->_ValidateSettings();

        return *resultPtr;
    }
    catch (const SettingsException& ex)
    {
        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        settings->_loadError = ex.Error();
        return *settings;
    }
    catch (const SettingsTypedDeserializationException& e)
    {
        auto settings{ winrt::make_self<implementation::CascadiaSettings>() };
        std::string_view what{ e.what() };
        settings->_deserializationErrorMessage = til::u8u16(what);
        return *settings;
    }
}

// Function Description:
// - Creates a new CascadiaSettings object initialized with settings from the
//   hardcoded defaults.json.
// Arguments:
// - <none>
// Return Value:
// - a unique_ptr to a CascadiaSettings with the settings from defaults.json
winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings CascadiaSettings::LoadDefaults()
{
    auto resultPtr{ winrt::make_self<CascadiaSettings>() };

    // We already have the defaults in memory, because we stamp them into a
    // header as part of the build process. We don't need to bother with reading
    // them from a file (and the potential that could fail)
    resultPtr->_ParseJsonString(DefaultJson, true);
    resultPtr->LayerJson(resultPtr->_defaultSettings);
    resultPtr->_ResolveDefaultProfile();

    return *resultPtr;
}

// Method Description:
// - Runs each of the configured dynamic profile generators (DPGs). Adds
//   profiles from any DPGs that ran to the end of our list of profiles.
// - Uses the Json::Value _userSettings to check which DPGs should not be run.
//   If the user settings has any namespaces in the "disabledProfileSources"
//   property, we'll ensure that any DPGs with a matching namespace _don't_ run.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_LoadDynamicProfiles()
{
    std::unordered_set<std::wstring> ignoredNamespaces;
    const auto disabledProfileSources = CascadiaSettings::_GetDisabledProfileSourcesJsonObject(_userSettings);
    if (disabledProfileSources.isArray())
    {
        for (const auto& json : disabledProfileSources)
        {
            ignoredNamespaces.emplace(JsonUtils::GetValue<std::wstring>(json));
        }
    }

    const GUID nullGuid{ 0 };
    for (auto& generator : _profileGenerators)
    {
        const std::wstring generatorNamespace{ generator->GetNamespace() };

        if (ignoredNamespaces.find(generatorNamespace) != ignoredNamespaces.end())
        {
            // namespace should be ignored
        }
        else
        {
            try
            {
                auto profiles = generator->GenerateProfiles();
                for (auto& profile : profiles)
                {
                    // If the profile did not have a GUID when it was generated,
                    // we'll synthesize a GUID for it in _ValidateProfilesHaveGuid
                    profile.Source(generatorNamespace);

                    _profiles.Append(profile);
                }
            }
            CATCH_LOG_MSG("Dynamic Profile Namespace: \"%ls\"", generatorNamespace.data());
        }
    }
}

// Method Description:
// - Attempts to read the given data as a string of JSON and parse that JSON
//   into a Json::Value.
// - Will ignore leading UTF-8 BOMs.
// - Additionally, will store the parsed JSON in this object, as either our
//   _defaultSettings or our _userSettings, depending on isDefaultSettings.
// - Does _not_ apply the json onto our current settings. Callers should make
//   sure to call LayerJson to ensure the settings are applied.
// Arguments:
// - fileData: the string to parse as JSON data
// - isDefaultSettings: if true, we should store the parsed JSON as our
//   defaultSettings. Otherwise, we'll store the parsed JSON as our user
//   settings.
// Return Value:
// - <none>
void CascadiaSettings::_ParseJsonString(std::string_view fileData, const bool isDefaultSettings)
{
    // Ignore UTF-8 BOM
    auto actualDataStart = fileData.data();
    const auto actualDataEnd = fileData.data() + fileData.size();
    if (fileData.compare(0, Utf8Bom.size(), Utf8Bom) == 0)
    {
        actualDataStart += Utf8Bom.size();
    }

    std::string errs; // This string will receive any error text from failing to parse.
    std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

    // Parse the json data into either our defaults or user settings. We'll keep
    // these original json values around for later, in case we need to parse
    // their raw contents again.
    Json::Value& root = isDefaultSettings ? _defaultSettings : _userSettings;
    // `parse` will return false if it fails.
    if (!reader->parse(actualDataStart, actualDataEnd, &root, &errs))
    {
        // This will be caught by App::_TryLoadSettings, who will display
        // the text to the user.
        throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
    }

    // If this is the user settings, also store away the original settings
    // string. We'll need to keep it around so we can modify it without
    // re-serializing their settings.
    if (!isDefaultSettings)
    {
        _userSettingsString = fileData;
    }
}

// Method Description:
// - Determines whether the user's settings file is missing a schema directive
//   and, if so, inserts one.
// - Assumes that the body of the root object is at an indentation of 4 spaces, and
//   therefore each member should be indented 4 spaces. If the user's settings
//   have a different indentation, we'll still insert valid json, it'll just be
//   indented incorrectly.
// Arguments:
// - <none>
// Return Value:
// - true iff we've made changes to the _userSettingsString that should be persisted.
bool CascadiaSettings::_PrependSchemaDirective()
{
    if (_userSettings.isMember(JsonKey(SchemaKey)))
    {
        return false;
    }

    // start points at the opening { for the root object.
    auto offset = _userSettings.getOffsetStart() + 1;
    _userSettingsString.insert(offset, SettingsSchemaFragment);
    offset += SettingsSchemaFragment.size();
    if (_userSettings.size() > 0)
    {
        _userSettingsString.insert(offset, ",");
    }
    return true;
}

// Method Description:
// - Finds all the dynamic profiles we've generated that _don't_ exist in the
//   user's settings. Generates a minimal blob of json for them, and inserts
//   them into the user's settings at the end of the list of profiles.
// - Does not reformat the user's settings file.
// - Does not write the file! Only modifies in-place the _userSettingsString
//   member. Callers should make sure to call
//   _WriteSettings(_userSettingsString) to make sure to persist these changes!
// - Assumes that the `profiles` object is at an indentation of 4 spaces, and
//   therefore each profile should be indented 8 spaces. If the user's settings
//   have a different indentation, we'll still insert valid json, it'll just be
//   indented incorrectly.
// Arguments:
// - <none>
// Return Value:
// - true iff we've made changes to the _userSettingsString that should be persisted.
bool CascadiaSettings::_AppendDynamicProfilesToUserSettings()
{
    // - Find the set of profiles that weren't either in the default profiles or
    //   in the user profiles. TODO:GH#2723 Do this in not O(N^2)
    // - For each of those profiles,
    //   * Diff them from the default profile
    //   * Serialize that diff
    //   * Insert that diff to the end of the list of profiles.

    Json::StreamWriterBuilder wbuilder;
    // Use 4 spaces to indent instead of \t
    wbuilder.settings_["indentation"] = "    ";
    wbuilder.settings_["enableYAMLCompatibility"] = true; // suppress spaces around colons

    auto isInJsonObj = [](const auto& profile, const auto& json) {
        for (auto profileJson : _GetProfilesJsonObject(json))
        {
            if (profileJson.isObject())
            {
                const auto profileImpl = winrt::get_self<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>(profile);
                if (profileImpl->ShouldBeLayered(profileJson))
                {
                    return true;
                }
                // If the profileJson doesn't have a GUID, then it might be in
                // the file still. We returned false because it shouldn't be
                // layered, but it might be a name-only profile.
            }
        }
        return false;
    };

    // Get the index in the user settings string of the _last_ profile.
    // We want to start inserting profiles immediately following the last profile.
    const auto userProfilesObj = _GetProfilesJsonObject(_userSettings);
    const auto numProfiles = userProfilesObj.size();
    const auto lastProfile = userProfilesObj[numProfiles - 1];
    size_t currentInsertIndex = lastProfile.getOffsetLimit();
    // Find the position of the first non-tab/space character before the last profile...
    const auto lastProfileIndentStartsAt{ _userSettingsString.find_last_not_of(" \t", lastProfile.getOffsetStart() - 1) };
    // ... and impute the user's preferred indentation.
    // (we're taking a copy because a string_view into a string we mutate is a no-no.)
    const std::string indentation{ _userSettingsString, lastProfileIndentStartsAt + 1, lastProfile.getOffsetStart() - lastProfileIndentStartsAt - 1 };

    bool changedFile = false;

    for (const auto& profile : _profiles)
    {
        if (!profile.HasGuid())
        {
            // If the profile doesn't have a guid, it's a name-only profile.
            // During validation, we'll generate a GUID for the profile, but
            // validation occurs after this. We should ignore these types of
            // profiles.
            // If a dynamic profile was generated _without_ a GUID, we also
            // don't want it serialized here. The first check in
            // Profile::ShouldBeLayered checks that the profile has a guid. For a
            // dynamic profile without a GUID, that'll _never_ be true, so it
            // would be impossible to be layered.
            continue;
        }

        // Skip profiles that are in the user settings or the default settings.
        if (isInJsonObj(profile, _userSettings) || isInJsonObj(profile, _defaultSettings))
        {
            continue;
        }

        // Generate a diff for the profile, that contains the minimal set of
        // changes to re-create this profile.
        const auto profileImpl = winrt::get_self<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>(profile);
        const auto diff = profileImpl->GenerateStub();
        auto profileSerialization = Json::writeString(wbuilder, diff);

        // Add the user's indent to the start of each line
        profileSerialization.insert(0, indentation);
        // Get the first newline
        size_t pos = profileSerialization.find("\n");
        // for each newline...
        while (pos != std::string::npos)
        {
            // Insert 8 spaces immediately following the current newline
            profileSerialization.insert(pos + 1, indentation);
            // Get the next newline
            pos = profileSerialization.find("\n", pos + indentation.size() + 1);
        }

        // Write a comma, newline to the file
        changedFile = true;
        _userSettingsString.insert(currentInsertIndex, ",");
        currentInsertIndex++;
        _userSettingsString.insert(currentInsertIndex, "\n");
        currentInsertIndex++;

        // Write the profile's serialization to the file
        _userSettingsString.insert(currentInsertIndex, profileSerialization);
        currentInsertIndex += profileSerialization.size();
    }

    return changedFile;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a CascadiaSettings object.
// Return Value:
// - a new CascadiaSettings instance created from the values in `json`
winrt::com_ptr<CascadiaSettings> CascadiaSettings::FromJson(const Json::Value& json)
{
    auto resultPtr = winrt::make_self<CascadiaSettings>();
    resultPtr->LayerJson(json);
    return resultPtr;
}

// Method Description:
// - Layer values from the given json object on top of the existing properties
//   of this object. For any keys we're expecting to be able to parse in the
//   given object, we'll parse them and replace our settings with values from
//   the new json object. Properties that _aren't_ in the json object will _not_
//   be replaced.
// Arguments:
// - json: an object which should be a partial serialization of a CascadiaSettings object.
// Return Value:
// <none>
void CascadiaSettings::LayerJson(const Json::Value& json)
{
    _globals->LayerJson(json);

    if (auto schemes{ json[SchemesKey.data()] })
    {
        for (auto schemeJson : schemes)
        {
            if (schemeJson.isObject())
            {
                _LayerOrCreateColorScheme(schemeJson);
            }
        }
    }

    for (auto profileJson : _GetProfilesJsonObject(json))
    {
        if (profileJson.isObject())
        {
            _LayerOrCreateProfile(profileJson);
        }
    }
}

// Method Description:
// - Given a partial json serialization of a Profile object, either layers that
//   json on a matching Profile we already have, or creates a new Profile
//   object from those settings.
// - For profiles that were created from a dynamic profile source, they'll have
//   both a guid and source guid that must both match. If a user profile with a
//   source set does not find a matching profile at load time, the profile
//   should be ignored.
// Arguments:
// - json: an object which may be a partial serialization of a Profile object.
// Return Value:
// - <none>
void CascadiaSettings::_LayerOrCreateProfile(const Json::Value& profileJson)
{
    // Layer the json on top of an existing profile, if we have one:
    auto pProfile = _FindMatchingProfile(profileJson);
    if (pProfile)
    {
        pProfile->LayerJson(profileJson);
    }
    else
    {
        // If this JSON represents a dynamic profile, we _shouldn't_ create the
        // profile here. We only want to create profiles for profiles without a
        // `source`. Dynamic profiles _must_ be layered on an existing profile.
        if (!Profile::IsDynamicProfileObject(profileJson))
        {
            auto profile = winrt::make_self<Profile>();

            // GH#2325: If we have a set of default profile settings, apply them here.
            // We _won't_ have these settings yet for defaults, dynamic profiles.
            if (_userDefaultProfileSettings)
            {
                profile->LayerJson(_userDefaultProfileSettings);
            }

            profile->LayerJson(profileJson);
            _profiles.Append(*profile);
        }
    }
}

// Method Description:
// - Finds a profile from our list of profiles that matches the given json
//   object. Uses Profile::ShouldBeLayered to determine if the Json::Value is a
//   match or not. This method should be used to find a profile to layer the
//   given settings upon.
// - Returns nullptr if no such match exists.
// Arguments:
// - json: an object which may be a partial serialization of a Profile object.
// Return Value:
// - a Profile that can be layered with the given json object, iff such a
//   profile exists.
winrt::com_ptr<Profile> CascadiaSettings::_FindMatchingProfile(const Json::Value& profileJson)
{
    for (auto profile : _profiles)
    {
        auto profileImpl = winrt::get_self<Profile>(profile);
        if (profileImpl->ShouldBeLayered(profileJson))
        {
            return profileImpl->get_strong();
        }
    }
    return nullptr;
}

// Method Description:
// - Finds the "default profile settings" if they exist in the users settings,
//   and applies them to the existing profiles. The "default profile settings"
//   are settings that should be applied to every profile a user has, with the
//   option of being overridden by explicit values in the profile. This should
//   be called _after_ the defaults have been parsed and dynamic profiles have
//   been generated, but before the other user profiles have been loaded.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_ApplyDefaultsFromUserSettings()
{
    // If `profiles` was an object, then look for the `defaults` object
    // underneath it for the default profile settings.
    auto defaultSettings{ Json::Value::null };
    if (const auto profiles{ _userSettings[JsonKey(ProfilesKey)] })
    {
        if (profiles.isObject())
        {
            defaultSettings = profiles[JsonKey(DefaultSettingsKey)];
        }
    }

    // cache and apply default profile settings
    // from user settings file
    if (defaultSettings)
    {
        _userDefaultProfileSettings = defaultSettings;

        // Remove the `guid` member from the default settings. That'll
        // hyper-explode, so just don't let them do that.
        _userDefaultProfileSettings.removeMember({ "guid" });

        for (auto profile : _profiles)
        {
            auto profileImpl = winrt::get_self<Profile>(profile);
            profileImpl->LayerJson(_userDefaultProfileSettings);
        }
    }
}

// Method Description:
// - Given a partial json serialization of a ColorScheme object, either layers that
//   json on a matching ColorScheme we already have, or creates a new ColorScheme
//   object from those settings.
// Arguments:
// - json: an object which should be a partial serialization of a ColorScheme object.
// Return Value:
// - <none>
void CascadiaSettings::_LayerOrCreateColorScheme(const Json::Value& schemeJson)
{
    // Layer the json on top of an existing profile, if we have one:
    auto pScheme = _FindMatchingColorScheme(schemeJson);
    if (pScheme)
    {
        pScheme->LayerJson(schemeJson);
    }
    else
    {
        const auto scheme = ColorScheme::FromJson(schemeJson);
        _globals->AddColorScheme(*scheme);
    }
}

// Method Description:
// - Finds a color scheme from our list of color schemes that matches the given
//   json object. Uses ColorScheme::GetNameFromJson to find the name and then
//   performs a lookup in the global map. This method should be used to find a
//   color scheme to layer the given settings upon.
// - Returns nullptr if no such match exists.
// Arguments:
// - json: an object which should be a partial serialization of a ColorScheme object.
// Return Value:
// - a ColorScheme that can be layered with the given json object, iff such a
//   color scheme exists.
winrt::com_ptr<ColorScheme> CascadiaSettings::_FindMatchingColorScheme(const Json::Value& schemeJson)
{
    if (auto schemeName = ColorScheme::GetNameFromJson(schemeJson))
    {
        if (auto scheme{ _globals->ColorSchemes().TryLookup(*schemeName) })
        {
            return winrt::get_self<ColorScheme>(scheme)->get_strong();
        }
    }
    return nullptr;
}

// Function Description:
// - Returns true if we're running in a packaged context.
//   If we are, we want to change our settings path slightly.
// Arguments:
// - <none>
// Return Value:
// - true iff we're running in a packaged context.
bool CascadiaSettings::_IsPackaged()
{
    UINT32 length = 0;
    LONG rc = GetCurrentPackageFullName(&length, nullptr);
    return rc != APPMODEL_ERROR_NO_PACKAGE;
}

// Method Description:
// - Writes the given content in UTF-8 to our settings file using the Win32 APIS's.
//   Will overwrite any existing content in the file.
// Arguments:
// - content: the given string of content to write to the file.
// Return Value:
// - <none>
//   This can throw an exception if we fail to open the file for writing, or we
//      fail to write the file
void CascadiaSettings::_WriteSettings(const std::string_view content)
{
    auto pathToSettingsFile{ CascadiaSettings::SettingsPath() };

    wil::unique_hfile hOut{ CreateFileW(pathToSettingsFile.c_str(),
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr) };
    if (!hOut)
    {
        THROW_LAST_ERROR();
    }
    THROW_LAST_ERROR_IF(!WriteFile(hOut.get(), content.data(), gsl::narrow<DWORD>(content.size()), nullptr, nullptr));
}

// Method Description:
// - Reads the content in UTF-8 encoding of our settings file using the Win32 APIs
// Arguments:
// - <none>
// Return Value:
// - an optional with the content of the file if we were able to open it,
//      otherwise the optional will be empty.
//   If the file exists, but we fail to read it, this can throw an exception
//      from reading the file
std::optional<std::string> CascadiaSettings::_ReadUserSettings()
{
    const auto pathToSettingsFile{ CascadiaSettings::SettingsPath() };
    wil::unique_hfile hFile{ CreateFileW(pathToSettingsFile.c_str(),
                                         GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL,
                                         nullptr) };

    if (!hFile)
    {
        // GH#5186 - We moved from profiles.json to settings.json; we want to
        // migrate any file we find. We're using MoveFile in case their settings.json
        // is a symbolic link.
        std::filesystem::path pathToLegacySettingsFile{ pathToSettingsFile.c_str() };
        pathToLegacySettingsFile.replace_filename(LegacySettingsFilename);

        wil::unique_hfile hLegacyFile{ CreateFileW(pathToLegacySettingsFile.c_str(),
                                                   GENERIC_READ,
                                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   nullptr,
                                                   OPEN_EXISTING,
                                                   FILE_ATTRIBUTE_NORMAL,
                                                   nullptr) };

        if (hLegacyFile)
        {
            // Close the file handle, move it, and re-open the file in its new location.
            hLegacyFile.reset();

            // Note: We're unsure if this is unsafe. Theoretically it's possible
            // that two instances of the app will try and move the settings file
            // simultaneously. We don't know what might happen in that scenario,
            // but we're also not sure how to safely lock the file to prevent
            // that from occurring.
            THROW_LAST_ERROR_IF(!MoveFile(pathToLegacySettingsFile.c_str(),
                                          pathToSettingsFile.c_str()));

            hFile.reset(CreateFileW(pathToSettingsFile.c_str(),
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr));

            // hFile shouldn't be INVALID. That's unexpected - We just moved the
            // file, we should be able to open it. Throw the error so we can get
            // some information here.
            THROW_LAST_ERROR_IF(!hFile);
        }
        else
        {
            // If the roaming file didn't exist, and the local file doesn't exist,
            //      that's fine. Just log the error and return nullopt - we'll
            //      create the defaults.
            LOG_LAST_ERROR();
            return std::nullopt;
        }
    }

    return _ReadFile(hFile.get());
}

// Method Description:
// - Reads the content in UTF-8 encoding of the given file using the Win32 APIs
// Arguments:
// - <none>
// Return Value:
// - an optional with the content of the file if we were able to read it. If we
//   fail to read it, this can throw an exception from reading the file
std::optional<std::string> CascadiaSettings::_ReadFile(HANDLE hFile)
{
    // fileSize is in bytes
    const auto fileSize = GetFileSize(hFile, nullptr);
    THROW_LAST_ERROR_IF(fileSize == INVALID_FILE_SIZE);

    auto utf8buffer = std::make_unique<char[]>(fileSize);

    DWORD bytesRead = 0;
    THROW_LAST_ERROR_IF(!ReadFile(hFile, utf8buffer.get(), fileSize, &bytesRead, nullptr));

    // convert buffer to UTF-8 string
    std::string utf8string(utf8buffer.get(), fileSize);

    return { utf8string };
}

// function Description:
// - Returns the full path to the settings file, either within the application
//   package, or in its unpackaged location. This path is under the "Local
//   AppData" folder, so it _doesn't_ roam to other machines.
// - If the application is unpackaged,
//   the file will end up under e.g. C:\Users\admin\AppData\Local\Microsoft\Windows Terminal\settings.json
// Arguments:
// - <none>
// Return Value:
// - the full path to the settings file
winrt::hstring CascadiaSettings::SettingsPath()
{
    wil::unique_cotaskmem_string localAppDataFolder;
    // KF_FLAG_FORCE_APP_DATA_REDIRECTION, when engaged, causes SHGet... to return
    // the new AppModel paths (Packages/xxx/RoamingState, etc.) for standard path requests.
    // Using this flag allows us to avoid Windows.Storage.ApplicationData completely.
    THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_FORCE_APP_DATA_REDIRECTION, nullptr, &localAppDataFolder));

    std::filesystem::path parentDirectoryForSettingsFile{ localAppDataFolder.get() };

    if (!_IsPackaged())
    {
        parentDirectoryForSettingsFile /= UnpackagedSettingsFolderName;
    }

    // Create the directory if it doesn't exist
    std::filesystem::create_directories(parentDirectoryForSettingsFile);

    return winrt::hstring{ (parentDirectoryForSettingsFile / SettingsFilename).wstring() };
}

winrt::hstring CascadiaSettings::DefaultSettingsPath()
{
    // Both of these posts suggest getting the path to the exe, then removing
    // the exe's name to get the package root:
    // * https://blogs.msdn.microsoft.com/appconsult/2017/06/23/accessing-to-the-files-in-the-installation-folder-in-a-desktop-bridge-application/
    // * https://blogs.msdn.microsoft.com/appconsult/2017/03/06/handling-data-in-a-converted-desktop-app-with-the-desktop-bridge/
    //
    // This would break if we ever moved our exe out of the package root.
    // HOWEVER, if we try to look for a defaults.json that's simply in the same
    // directory as the exe, that will work for unpackaged scenarios as well. So
    // let's try that.

    HMODULE hModule = GetModuleHandle(nullptr);
    THROW_LAST_ERROR_IF(hModule == nullptr);

    std::wstring exePathString;
    THROW_IF_FAILED(wil::GetModuleFileNameW(hModule, exePathString));

    const std::filesystem::path exePath{ exePathString };
    const std::filesystem::path rootDir = exePath.parent_path();
    return winrt::hstring{ (rootDir / DefaultsFilename).wstring() };
}

// Function Description:
// - Gets the object in the given JSON object under the "profiles" key. Returns
//   null if there's no "profiles" key.
// Arguments:
// - json: the json object to get the profiles from.
// Return Value:
// - the Json::Value representing the profiles property from the given object
const Json::Value& CascadiaSettings::_GetProfilesJsonObject(const Json::Value& json)
{
    const auto& profilesProperty = json[JsonKey(ProfilesKey)];
    return profilesProperty.isArray() ?
               profilesProperty :
               profilesProperty[JsonKey(ProfilesListKey)];
}

// Function Description:
// - Gets the object in the given JSON object under the "disabledProfileSources"
//   key. Returns null if there's no "disabledProfileSources" key.
// Arguments:
// - json: the json object to get the disabled profile sources from.
// Return Value:
// - the Json::Value representing the `disabledProfileSources` property from the
//   given object
const Json::Value& CascadiaSettings::_GetDisabledProfileSourcesJsonObject(const Json::Value& json)
{
    if (!json)
    {
        return Json::Value::nullSingleton();
    }
    return json[JsonKey(DisabledProfileSourcesKey)];
}
