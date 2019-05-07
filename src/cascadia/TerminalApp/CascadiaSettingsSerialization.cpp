// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"
#include <appmodel.h>
#include <wil/com.h>
#include <wil/filesystem.h>
#include <shlobj.h>

using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace ::Microsoft::Console;

static const std::wstring FILENAME { L"profiles.json" };
static const std::wstring SETTINGS_FOLDER_NAME{ L"\\Microsoft\\Windows Terminal\\" };

static const std::wstring PROFILES_KEY{ L"profiles" };
static const std::wstring KEYBINDINGS_KEY{ L"keybindings" };
static const std::wstring SCHEMES_KEY{ L"schemes" };

// Method Description:
// - Creates a CascadiaSettings from whatever's saved on disk, or instantiates
//      a new one with the default values. If we're running as a packaged app,
//      it will load the settings from our packaged localappdata. If we're
//      running as an unpackaged application, it will read it from the path
//      we've set under localappdata.
// Arguments:
// - saveOnLoad: If true, we'll write the settings back out after we load them,
//   to make sure the schema is updated.
// Return Value:
// - a unique_ptr containing a new CascadiaSettings object.
std::unique_ptr<CascadiaSettings> CascadiaSettings::LoadAll(const bool saveOnLoad)
{
    std::unique_ptr<CascadiaSettings> resultPtr;
    std::optional<winrt::hstring> fileData = _IsPackaged() ?
                                             _LoadAsPackagedApp() : _LoadAsUnpackagedApp();

    const bool foundFile = fileData.has_value();
    if (foundFile)
    {
        const auto actualData = fileData.value();

        JsonObject root{ nullptr };
        // bool parsedSuccessfully = JsonValue::TryParse(actualData, root);
        root = JsonObject::Parse(actualData); bool parsedSuccessfully = true;
        // TODO:MSFT:20737698 - Display an error if we failed to parse settings
        if (parsedSuccessfully)
        {
            JsonObject obj = root;//.GetObjectW();
            resultPtr = FromJson(obj);

            //  Update profile only if it has changed.
            if (saveOnLoad)
            {
                const JsonObject json = resultPtr->ToJson();
                auto serializedSettings = json.Stringify();

                if (actualData != serializedSettings)
                {
                    resultPtr->SaveAll();
                }
            }
        }
        else
        {
            // Until 20737698 is done, throw an error, so debugging can trace
            //      the exception here, instead of later on in unrelated code
            THROW_HR(E_INVALIDARG);
        }
    }
    else
    {
        resultPtr = std::make_unique<CascadiaSettings>();
        resultPtr->CreateDefaults();

        // The settings file does not exist.  Let's commit one.
        resultPtr->SaveAll();
    }

    return resultPtr;
}

// Method Description:
// - Serialize this settings structure, and save it to a file. The location of
//      the file changes depending whether we're running as a packaged
//      application or not.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::SaveAll() const
{
    const JsonObject json = ToJson();
    auto serializedSettings = json.Stringify();

    if (_IsPackaged())
    {
        _SaveAsPackagedApp(serializedSettings);
    }
    else
    {
        _SaveAsUnpackagedApp(serializedSettings);
    }
}

// Method Description:
// - Serialize this object to a JsonObject.
// Arguments:
// - <none>
// Return Value:
// - a JsonObject which is an equivalent serialization of this object.
JsonObject CascadiaSettings::ToJson() const
{
    // _globals.ToJson will initialize the settings object will all the global
    // settings in the root of the object.
    winrt::Windows::Data::Json::JsonObject jsonObject = _globals.ToJson();

    JsonArray schemesArray{};
    const auto& colorSchemes = _globals.GetColorSchemes();
    for (auto& scheme : colorSchemes)
    {
        schemesArray.Append(scheme.ToJson());
    }

    JsonArray profilesArray{};
    for (auto& profile : _profiles)
    {
        profilesArray.Append(profile.ToJson());
    }

    jsonObject.Insert(PROFILES_KEY, profilesArray);
    jsonObject.Insert(SCHEMES_KEY, schemesArray);

    return jsonObject;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a CascadiaSettings object.
// Return Value:
// - a new CascadiaSettings instance created from the values in `json`
std::unique_ptr<CascadiaSettings> CascadiaSettings::FromJson(JsonObject json)
{
    std::unique_ptr<CascadiaSettings> resultPtr = std::make_unique<CascadiaSettings>();

    resultPtr->_globals = GlobalAppSettings::FromJson(json);

    // TODO:MSFT:20737698 - Display an error if we failed to parse settings
    // What should we do here if these keys aren't found?For default profile,
    //      we could always pick the first  profile and just set that as the default.
    // Finding no schemes is probably fine, unless of course one profile
    //      references a scheme.  We could fail with come error saying the
    //      profiles file is corrupted.
    // Not having any profiles is also bad - should we say the file is corrupted?
    //      Or should we just recreate the default profiles?

    auto& resultSchemes = resultPtr->_globals.GetColorSchemes();
    if (json.HasKey(SCHEMES_KEY))
    {
        auto schemes = json.GetNamedArray(SCHEMES_KEY);
        for (auto schemeJson : schemes)
        {
            if (schemeJson.ValueType() == JsonValueType::Object)
            {
                auto schemeObj = schemeJson.GetObjectW();
                auto scheme = ColorScheme::FromJson(schemeObj);
                resultSchemes.emplace_back(std::move(scheme));
            }
        }
    }

    if (json.HasKey(PROFILES_KEY))
    {
        auto profiles = json.GetNamedArray(PROFILES_KEY);
        for (auto profileJson : profiles)
        {
            if (profileJson.ValueType() == JsonValueType::Object)
            {
                auto profileObj = profileJson.GetObjectW();
                auto profile = Profile::FromJson(profileObj);
                resultPtr->_profiles.emplace_back(std::move(profile));
            }
        }
    }

    // TODO:MSFT:20700157
    // Load the keybindings from the file as well
    resultPtr->_CreateDefaultKeybindings();

    return resultPtr;
}

// Function Description:
// - Returns true if we're running in a packaged context. If we are, then we
//      have to use the Windows.Storage API's to save/load our files. If we're
//      not, then we won't be able to use those API's.
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
// - Writes the given content to our settings file as UTF-8 encoded using the Windows.Storage
//      APIS's. This will only work within the context of an application with
//      package identity, so make sure to call _IsPackaged before calling this method.
//   Will overwrite any existing content in the file.
// Arguments:
// - content: the given string of content to write to the file.
// Return Value:
// - <none>
void CascadiaSettings::_SaveAsPackagedApp(const winrt::hstring content)
{
    auto curr = ApplicationData::Current();
    auto folder = curr.RoamingFolder();

    auto file_async = folder.CreateFileAsync(FILENAME,
                                             CreationCollisionOption::ReplaceExisting);

    auto file = file_async.get();

    DataWriter dw = DataWriter();

    // DataWriter will convert UTF-16 string to UTF-8 (expected settings file encoding)
    dw.UnicodeEncoding(UnicodeEncoding::Utf8);

    dw.WriteString(content);

    FileIO::WriteBufferAsync(file, dw.DetachBuffer()).get();
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
void CascadiaSettings::_SaveAsUnpackagedApp(const winrt::hstring content)
{
    // convert UTF-16 to UTF-8
    auto contentString = winrt::to_string(content);

    // Get path to output file
    // In this scenario, the settings file will end up under e.g. C:\Users\admin\AppData\Roaming\Microsoft\Windows Terminal\profiles.json
    std::wstring pathToSettingsFile = CascadiaSettings::_GetFullPathToUnpackagedSettingsFile();

    auto hOut = CreateFileW(pathToSettingsFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        THROW_LAST_ERROR();
    }
    THROW_LAST_ERROR_IF(!WriteFile(hOut, contentString.c_str(), gsl::narrow<DWORD>(contentString.length()), 0, 0));
    CloseHandle(hOut);
}

// Method Description:
// - Computes the path to the settings file if the app is run unpackaged.
//   Will create any intermediate directories if they don't exist.
//   The file will end up under e.g. C:\Users\admin\AppData\Roaming\Microsoft\Windows Terminal\profiles.json
// Arguments:
// - <none>
// Return Value:
// - A string containing the path to the unpackaged settings file
//   This can throw an exception if it fails to get the roaming app data folder.
std::wstring CascadiaSettings::_GetFullPathToUnpackagedSettingsFile()
{
    wil::unique_cotaskmem_string roamingAppDataFolder;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &roamingAppDataFolder)))
    {
        THROW_LAST_ERROR();
    }

    std::wstring parentDirectoryForSettingsFile(roamingAppDataFolder.get());
    parentDirectoryForSettingsFile.append(SETTINGS_FOLDER_NAME);

    // Create the directory if it doesn't exist
    wil::CreateDirectoryDeep(parentDirectoryForSettingsFile.c_str());

    std::wstring pathToSettingsFile(parentDirectoryForSettingsFile);
    pathToSettingsFile.append(FILENAME);

    return pathToSettingsFile;
}

// Method Description:
// - Reads the content of our settings file using the Windows.Storage
//      APIS's. This will only work within the context of an application with
//      package identity, so make sure to call _IsPackaged before calling this method.
// Arguments:
// - <none>
// Return Value:
// - an optional with the content of the file if we were able to open it,
//      otherwise the optional will be empty
std::optional<winrt::hstring> CascadiaSettings::_LoadAsPackagedApp()
{
    auto curr = ApplicationData::Current();
    auto folder = curr.RoamingFolder();
    auto file_async = folder.TryGetItemAsync(FILENAME);
    auto file = file_async.get();

    if (file == nullptr)
    {
        return std::nullopt;
    }
    const auto storageFile = file.as<StorageFile>();

    // settings file is UTF-8 without BOM
    return { FileIO::ReadTextAsync(storageFile, UnicodeEncoding::Utf8).get() };
}


// Method Description:
// - Reads the content in UTF-8 enconding of our settings file using the Win32 APIs
// Arguments:
// - <none>
// Return Value:
// - an optional with the content of the file if we were able to open it,
//      otherwise the optional will be empty.
//   If the file exists, but we fail to read it, this can throw an exception
//      from reading the file
std::optional<winrt::hstring> CascadiaSettings::_LoadAsUnpackagedApp()
{
    std::wstring pathToSettingsFile = CascadiaSettings::_GetFullPathToUnpackagedSettingsFile();
    const auto hFile = CreateFileW(pathToSettingsFile.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // If the file doesn't exist, that's fine. Just log the error and return
        //      nullopt - we'll create the defaults.
        LOG_LAST_ERROR();
        return std::nullopt;
    }

    // fileSize is in bytes
    const auto fileSize = GetFileSize(hFile, nullptr);
    THROW_LAST_ERROR_IF(fileSize == INVALID_FILE_SIZE);

    auto utf8buffer = std::make_unique<char[]>(fileSize);

    DWORD bytesRead = 0;
    THROW_LAST_ERROR_IF(!ReadFile(hFile, utf8buffer.get(), fileSize, &bytesRead, nullptr));
    CloseHandle(hFile);

    // convert buffer to UTF-8 string
    std::string utf8string(utf8buffer.get(), fileSize);

    // UTF-8 to UTF-16
    const winrt::hstring fileData = winrt::to_hstring(utf8string);

    return { fileData };
}

// function Description:
// - Returns the full path to the settings file, either within the application
//   package, or in it's unpackaged location.
// Arguments:
// - <none>
// Return Value:
// - the full path to the settings file
winrt::hstring CascadiaSettings::GetSettingsPath()
{
    return _IsPackaged() ? CascadiaSettings::_GetPackagedSettingsPath() :
                           winrt::hstring{ CascadiaSettings::_GetFullPathToUnpackagedSettingsFile() };
}

// Function Description:
// - Get the full path to settings file in it's packaged location.
// Arguments:
// - <none>
// Return Value:
// - the full path to the packaged settings file.
winrt::hstring CascadiaSettings::_GetPackagedSettingsPath()
{
    const auto curr = ApplicationData::Current();
    const auto folder = curr.RoamingFolder();
    const auto file_async = folder.TryGetItemAsync(FILENAME);
    const auto file = file_async.get();
    return file.Path();
}
