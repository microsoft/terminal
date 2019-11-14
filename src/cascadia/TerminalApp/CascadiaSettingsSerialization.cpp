// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"
#include "utils.h"
#include "JsonUtils.h"
#include <appmodel.h>
#include <shlobj.h>

// defaults.h is a file containing the default json settings in a std::string_view
#include "defaults.h"
// userDefault.h is like the above, but with a default template for the user's profiles.json.
#include "userDefaults.h"
// Both defaults.h and userDefaults.h are generated at build time into the
// "Generated Files" directory.

using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace ::Microsoft::Console;

static constexpr std::wstring_view SettingsFilename{ L"profiles.json" };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };

static constexpr std::wstring_view DefaultsFilename{ L"defaults.json" };

static constexpr std::string_view SchemaKey{ "$schema" };
static constexpr std::string_view ProfilesKey{ "profiles" };
static constexpr std::string_view KeybindingsKey{ "keybindings" };
static constexpr std::string_view GlobalsKey{ "globals" };
static constexpr std::string_view SchemesKey{ "schemes" };

static constexpr std::string_view DisabledProfileSourcesKey{ "disabledProfileSources" };

static constexpr std::string_view Utf8Bom{ u8"\uFEFF" };
static constexpr std::string_view DefaultProfilesIndentation{ "        " };
static constexpr std::string_view SettingsSchemaFragment{ "\n"
                                                          R"(    "$schema": "https://aka.ms/terminal-profiles-schema")" };

// Method Description:
// - Creates a CascadiaSettings from whatever's saved on disk, or instantiates
//      a new one with the default values. If we're running as a packaged app,
//      it will load the settings from our packaged localappdata. If we're
//      running as an unpackaged application, it will read it from the path
//      we've set under localappdata.
// - Loads both the settings from the defaults.json and the user's profiles.json
// - Also runs and dynamic profile generators. If any of those generators create
//   new profiles, we'll write the user settings back to the file, with the new
//   profiles inserted into their list of profiles.
// Return Value:
// - a unique_ptr containing a new CascadiaSettings object.
std::unique_ptr<CascadiaSettings> CascadiaSettings::LoadAll()
{
    auto resultPtr = LoadDefaults();

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
    else
    {
        // We didn't find the user settings. We'll need to create a file
        // to use as the user defaults.
        // For now, just parse our user settings template as their user settings.
        resultPtr->_ParseJsonString(UserSettingsJson, false);
        needToWriteFile = true;
    }

    // Load profiles from dynamic profile generators. _userSettings should be
    // created by now, because we're going to check in there for any generators
    // that should be disabled.
    resultPtr->_LoadDynamicProfiles();

    // Apply the user's settings
    resultPtr->LayerJson(resultPtr->_userSettings);

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

    return resultPtr;
}

// Function Description:
// - Creates a new CascadiaSettings object initialized with settings from the
//   hardcoded defaults.json.
// Arguments:
// - <none>
// Return Value:
// - a unique_ptr to a CascadiaSettings with the settings from defaults.json
std::unique_ptr<CascadiaSettings> CascadiaSettings::LoadDefaults()
{
    auto resultPtr = std::make_unique<CascadiaSettings>();

    // We already have the defaults in memory, because we stamp them into a
    // header as part of the build process. We don't need to bother with reading
    // them from a file (and the potential that could fail)
    resultPtr->_ParseJsonString(DefaultJson, true);
    resultPtr->LayerJson(resultPtr->_defaultSettings);

    return resultPtr;
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
        for (const auto& ns : disabledProfileSources)
        {
            ignoredNamespaces.emplace(GetWstringFromJson(ns));
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
                    profile.SetSource(generatorNamespace);

                    _profiles.emplace_back(profile);
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

    std::string errs; // This string will recieve any error text from failing to parse.
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

    const Profile defaultProfile;

    Json::StreamWriterBuilder wbuilder;
    // Use 4 spaces to indent instead of \t
    wbuilder.settings_["indentation"] = "    ";
    wbuilder.settings_["enableYAMLCompatibility"] = true; // suppress spaces around colons

    auto isInJsonObj = [](const auto& profile, const auto& json) {
        for (auto profileJson : _GetProfilesJsonObject(json))
        {
            if (profileJson.isObject())
            {
                if (profile.ShouldBeLayered(profileJson))
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
        const auto diff = profile.GenerateStub();
        auto profileSerialization = Json::writeString(wbuilder, diff);

        // Add 8 spaces to the start of each line
        profileSerialization.insert(0, DefaultProfilesIndentation);
        // Get the first newline
        size_t pos = profileSerialization.find("\n");
        // for each newline...
        while (pos != std::string::npos)
        {
            // Insert 8 spaces immediately following the current newline
            profileSerialization.insert(pos + 1, DefaultProfilesIndentation);
            // Get the next newline
            pos = profileSerialization.find("\n", pos + 9);
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
std::unique_ptr<CascadiaSettings> CascadiaSettings::FromJson(const Json::Value& json)
{
    auto resultPtr = std::make_unique<CascadiaSettings>();
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
    // microsoft/terminal#2906: First layer the root object as our globals. If
    // there is also a `globals` object, layer that one on top of the settings
    // from the root.
    _globals.LayerJson(json);

    if (auto globals{ json[GlobalsKey.data()] })
    {
        if (globals.isObject())
        {
            _globals.LayerJson(globals);
        }
    }

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
            auto profile = Profile::FromJson(profileJson);
            _profiles.emplace_back(profile);
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
Profile* CascadiaSettings::_FindMatchingProfile(const Json::Value& profileJson)
{
    for (auto& profile : _profiles)
    {
        if (profile.ShouldBeLayered(profileJson))
        {
            // HERE BE DRAGONS: Returning a pointer to a type in the vector is
            // maybe not the _safest_ thing, but we have a mind to make Profile
            // and ColorScheme winrt types in the future, so this will be safer
            // then.
            return &profile;
        }
    }
    return nullptr;
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
        _globals.AddColorScheme(ColorScheme::FromJson(schemeJson));
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
ColorScheme* CascadiaSettings::_FindMatchingColorScheme(const Json::Value& schemeJson)
{
    if (auto schemeName = ColorScheme::GetNameFromJson(schemeJson))
    {
        auto& schemes = _globals.GetColorSchemes();
        auto iterator = schemes.find(*schemeName);
        if (iterator != schemes.end())
        {
            // HERE BE DRAGONS: Returning a pointer to a type in the vector is
            // maybe not the _safest_ thing, but we have a mind to make Profile
            // and ColorScheme winrt types in the future, so this will be safer
            // then.
            return &iterator->second;
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
    LONG rc = GetCurrentPackageFullName(&length, NULL);
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
    auto pathToSettingsFile{ CascadiaSettings::GetSettingsPath() };

    wil::unique_hfile hOut{ CreateFileW(pathToSettingsFile.c_str(),
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL) };
    if (!hOut)
    {
        THROW_LAST_ERROR();
    }
    THROW_LAST_ERROR_IF(!WriteFile(hOut.get(), content.data(), gsl::narrow<DWORD>(content.size()), 0, 0));
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
    const auto pathToSettingsFile{ CascadiaSettings::GetSettingsPath() };
    wil::unique_hfile hFile{ CreateFileW(pathToSettingsFile.c_str(),
                                         GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL,
                                         nullptr) };

    if (!hFile)
    {
        // GH#1770 - Now that we're _not_ roaming our settings, do a quick check
        // to see if there's a file in the Roaming App data folder. If there is
        // a file there, but not in the LocalAppData, it's likely the user is
        // upgrading from a version of the terminal from before this change.
        // We'll try moving the file from the Roaming app data folder to the
        // local appdata folder.

        const auto pathToRoamingSettingsFile{ CascadiaSettings::GetSettingsPath(true) };
        wil::unique_hfile hRoamingFile{ CreateFileW(pathToRoamingSettingsFile.c_str(),
                                                    GENERIC_READ,
                                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                    nullptr,
                                                    OPEN_EXISTING,
                                                    FILE_ATTRIBUTE_NORMAL,
                                                    nullptr) };

        if (hRoamingFile)
        {
            // Close the file handle, move it, and re-open the file in its new location.
            hRoamingFile.reset();

            // Note: We're unsure if this is unsafe. Theoretically it's possible
            // that two instances of the app will try and move the settings file
            // simultaneously. We don't know what might happen in that scenario,
            // but we're also not sure how to safely lock the file to prevent
            // that from ocurring.
            THROW_LAST_ERROR_IF(!MoveFile(pathToRoamingSettingsFile.c_str(),
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
//   the file will end up under e.g. C:\Users\admin\AppData\Local\Microsoft\Windows Terminal\profiles.json
// Arguments:
// - <none>
// Return Value:
// - the full path to the settings file
std::wstring CascadiaSettings::GetSettingsPath(const bool useRoamingPath)
{
    wil::unique_cotaskmem_string localAppDataFolder;
    // KF_FLAG_FORCE_APP_DATA_REDIRECTION, when engaged, causes SHGet... to return
    // the new AppModel paths (Packages/xxx/RoamingState, etc.) for standard path requests.
    // Using this flag allows us to avoid Windows.Storage.ApplicationData completely.
    const auto knowFolderId = useRoamingPath ? FOLDERID_RoamingAppData : FOLDERID_LocalAppData;
    if (FAILED(SHGetKnownFolderPath(knowFolderId, KF_FLAG_FORCE_APP_DATA_REDIRECTION, 0, &localAppDataFolder)))
    {
        THROW_LAST_ERROR();
    }

    std::filesystem::path parentDirectoryForSettingsFile{ localAppDataFolder.get() };

    if (!_IsPackaged())
    {
        parentDirectoryForSettingsFile /= UnpackagedSettingsFolderName;
    }

    // Create the directory if it doesn't exist
    std::filesystem::create_directories(parentDirectoryForSettingsFile);

    return parentDirectoryForSettingsFile / SettingsFilename;
}

std::wstring CascadiaSettings::GetDefaultSettingsPath()
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
    return rootDir / DefaultsFilename;
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
    return json[JsonKey(ProfilesKey)];
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
    // Check the globals first, then look in the root.
    if (json.isMember(JsonKey(GlobalsKey)))
    {
        return json[JsonKey(GlobalsKey)][JsonKey(DisabledProfileSourcesKey)];
    }
    return json[JsonKey(DisabledProfileSourcesKey)];
}
