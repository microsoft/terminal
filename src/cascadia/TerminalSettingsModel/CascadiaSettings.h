/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.h

Abstract:
- This class acts as the container for all app settings. It's composed of two
        parts: Globals, which are app-wide settings, and Profiles, which contain
        a set of settings that apply to a single instance of the terminal.
  Also contains the logic for serializing and deserializing this object.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "CascadiaSettings.g.h"
#include "FragmentSettings.g.h"
#include "FragmentProfileEntry.g.h"
#include "FragmentColorSchemeEntry.g.h"
#include "ExtensionPackage.g.h"

#include "GlobalAppSettings.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class IDynamicProfileGenerator;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    std::string_view LoadStringResource(int resourceID);
    winrt::com_ptr<Profile> CreateChild(const winrt::com_ptr<Profile>& parent);

    class SettingsTypedDeserializationException final : public std::runtime_error
    {
    public:
        SettingsTypedDeserializationException(const char* message) noexcept :
            std::runtime_error(message) {}
    };

    struct ExtensionPackage : ExtensionPackageT<ExtensionPackage>
    {
    public:
        ExtensionPackage(hstring source, FragmentScope scope) :
            _source{ source },
            _scope{ scope },
            _fragments{ winrt::single_threaded_vector<Model::FragmentSettings>() } {}

        hstring Source() const noexcept { return _source; }
        FragmentScope Scope() const noexcept { return _scope; }
        Windows::Foundation::Collections::IVectorView<Model::FragmentSettings> FragmentsView() const noexcept { return _fragments.GetView(); }
        Windows::Foundation::Collections::IVector<Model::FragmentSettings> Fragments() const noexcept { return _fragments; }

        WINRT_PROPERTY(hstring, Icon);
        WINRT_PROPERTY(hstring, DisplayName);

    private:
        hstring _source;
        FragmentScope _scope;
        Windows::Foundation::Collections::IVector<Model::FragmentSettings> _fragments;
    };

    struct ParsedSettings
    {
        winrt::com_ptr<implementation::GlobalAppSettings> globals;
        winrt::com_ptr<implementation::Profile> baseLayerProfile;
        std::vector<winrt::com_ptr<implementation::Profile>> profiles;
        std::unordered_map<winrt::guid, winrt::com_ptr<implementation::Profile>> profilesByGuid;
        std::unordered_map<winrt::hstring, winrt::com_ptr<implementation::ColorScheme>> colorSchemes;
        std::unordered_map<winrt::hstring, winrt::hstring> colorSchemeRemappings;
        bool fixupsAppliedDuringLoad{ false };
        std::set<std::string> themesChangeLog;

        void clear();
    };

    struct SettingsLoader
    {
        static SettingsLoader Default(const std::string_view& userJSON, const std::string_view& inboxJSON);
        static std::vector<Model::ExtensionPackage> LoadExtensionPackages();
        SettingsLoader(const std::string_view& userJSON, const std::string_view& inboxJSON);

        void GenerateProfiles();
        void GenerateExtensionPackagesFromProfileGenerators();
        void ApplyRuntimeInitialSettings();
        void MergeInboxIntoUserSettings();
        void FindFragmentsAndMergeIntoUserSettings(bool generateExtensionPackages);
        void MergeFragmentIntoUserSettings(const winrt::hstring& source, const winrt::hstring& basePath, const std::string_view& content);
        void FinalizeLayering();
        bool DisableDeletedProfiles();
        bool AddDynamicProfileFolders();
        bool RemapColorSchemeForProfile(const winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>& profile);
        bool FixupUserSettings();

        ParsedSettings inboxSettings;
        ParsedSettings userSettings;
        std::unordered_map<hstring, winrt::com_ptr<implementation::ExtensionPackage>> extensionPackageMap;
        bool duplicateProfile = false;
        bool sshProfilesGenerated = false;

    private:
        struct JsonSettings
        {
            Json::Value root;
            const Json::Value& colorSchemes;
            const Json::Value& profileDefaults;
            const Json::Value& profilesList;
            const Json::Value& themes;
        };
        struct ParseFragmentMetadata
        {
            std::wstring_view jsonFilename;
            FragmentScope scope;
        };
        SettingsLoader() = default;

        static std::pair<size_t, size_t> _lineAndColumnFromPosition(const std::string_view& string, const size_t position);
        static void _rethrowSerializationExceptionWithLocationInfo(const JsonUtils::DeserializationError& e, const std::string_view& settingsString);
        static Json::Value _parseJSON(const std::string_view& content);
        static const Json::Value& _getJSONValue(const Json::Value& json, const std::string_view& key) noexcept;
        std::span<const winrt::com_ptr<implementation::Profile>> _getNonUserOriginProfiles() const;
        void _parse(const OriginTag origin, const winrt::hstring& source, const std::string_view& content, ParsedSettings& settings);
        void _parseFragment(const winrt::hstring& source, const winrt::hstring& sourceBasePath, const std::string_view& content, ParsedSettings& settings, const std::optional<ParseFragmentMetadata>& fragmentMeta);
        static JsonSettings _parseJson(const std::string_view& content);
        static winrt::com_ptr<implementation::Profile> _parseProfile(const OriginTag origin, const winrt::hstring& source, const Json::Value& profileJson);
        void _appendProfile(winrt::com_ptr<Profile>&& profile, const winrt::guid& guid, ParsedSettings& settings);
        void _addUserProfileParent(const winrt::com_ptr<implementation::Profile>& profile);
        bool _addOrMergeUserColorScheme(const winrt::com_ptr<implementation::ColorScheme>& colorScheme);
        static void _executeGenerator(const IDynamicProfileGenerator& generator, std::vector<winrt::com_ptr<implementation::Profile>>& profilesList);
        winrt::com_ptr<implementation::ExtensionPackage> _registerFragment(const winrt::Microsoft::Terminal::Settings::Model::FragmentSettings& fragment, FragmentScope scope);
        Json::StreamWriterBuilder _getJsonStyledWriter();

        std::unordered_set<winrt::hstring, til::transparent_hstring_hash, til::transparent_hstring_equal_to> _ignoredNamespaces;
        std::set<std::string> themesChangeLog;
        // See _getNonUserOriginProfiles().
        size_t _userProfileCount = 0;
    };

    struct CascadiaSettings : CascadiaSettingsT<CascadiaSettings>
    {
    public:
        static Model::CascadiaSettings LoadDefaults();
        static Model::CascadiaSettings LoadAll();

        static winrt::hstring SettingsDirectory();
        static winrt::hstring SettingsPath();
        static winrt::hstring DefaultSettingsPath();
        static winrt::hstring ApplicationDisplayName();
        static winrt::hstring ApplicationVersion();
        static bool IsPortableMode();

        CascadiaSettings() noexcept = default;
        CascadiaSettings(const winrt::hstring& userJSON, const winrt::hstring& inboxJSON);
        CascadiaSettings(const std::string_view& userJSON, const std::string_view& inboxJSON = {});
        explicit CascadiaSettings(SettingsLoader&& loader);

        // user settings
        winrt::hstring Hash() const noexcept;
        Model::CascadiaSettings Copy() const;
        Model::GlobalAppSettings GlobalSettings() const;
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> AllProfiles() const noexcept;
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> ActiveProfiles() const noexcept;
        Model::ActionMap ActionMap() const noexcept;
        winrt::Windows::Foundation::Collections::IVectorView<Model::ExtensionPackage> Extensions();
        void ResetApplicationState() const;
        void ResetToDefaultSettings();
        bool WriteSettingsToDisk();
        Json::Value ToJson() const;
        Model::Profile ProfileDefaults() const;
        Model::Profile CreateNewProfile();
        Model::Profile FindProfile(const winrt::guid& guid) const noexcept;
        void UpdateColorSchemeReferences(const winrt::hstring& oldName, const winrt::hstring& newName);
        Model::Profile GetProfileForArgs(const Model::NewTerminalArgs& newTerminalArgs) const;
        Model::Profile GetProfileByName(const winrt::hstring& name) const;
        Model::Profile GetProfileByIndex(uint32_t index) const;
        Model::Profile DuplicateProfile(const Model::Profile& source);

        // load errors
        winrt::Windows::Foundation::Collections::IVectorView<Model::SettingsLoadWarnings> Warnings() const;
        winrt::Windows::Foundation::IReference<Model::SettingsLoadErrors> GetLoadingError() const;
        winrt::hstring GetSerializationErrorMessage() const;

        // defterm
        static bool IsDefaultTerminalAvailable() noexcept;
        static bool IsDefaultTerminalSet() noexcept;
        winrt::Windows::Foundation::Collections::IObservableVector<Model::DefaultTerminal> DefaultTerminals() noexcept;
        Model::DefaultTerminal CurrentDefaultTerminal() noexcept;
        void CurrentDefaultTerminal(const Model::DefaultTerminal& terminal);

        void ExpandCommands();
        void UpdateCommandID(const Model::Command& cmd, winrt::hstring newID);
        void ResolveMediaResources() { _validateMediaResources(); }

        void LogSettingChanges(bool isJsonLoad) const;

    private:
        static const std::filesystem::path& _settingsPath();
        static const std::filesystem::path& _releaseSettingsPath();
        static winrt::hstring _calculateHash(std::string_view settings, const FILETIME& lastWriteTime);

        winrt::com_ptr<implementation::Profile> _createNewProfile(const std::wstring_view& name) const;
        Model::Profile _getProfileForCommandLine(const winrt::hstring& commandLine) const;
        void _refreshDefaultTerminals();
        void _writeSettingsToDisk(std::string_view contents);

        void _resolveDefaultProfile() const;
        void _resolveNewTabMenuProfiles() const;
        void _resolveNewTabMenuProfilesSet(const winrt::Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> entries, winrt::Windows::Foundation::Collections::IMap<int, Model::Profile>& remainingProfiles, Model::RemainingProfilesEntry& remainingProfilesEntry) const;

        void _validateSettings();
        void _validateAllSchemesExist();
        void _resolveSingleMediaResource(OriginTag origin, std::wstring_view basePath, const Model::IMediaResource& resource);
        void _validateMediaResources();
        void _validateProfileEnvironmentVariables();
        void _validateKeybindings() const;
        void _validateColorSchemesInCommands() const;
        bool _hasInvalidColorScheme(const Model::Command& command) const;
        void _validateThemeExists();
        void _validateRegexes();

        void _researchOnLoad();

        // user settings
        winrt::hstring _hash;
        winrt::com_ptr<implementation::GlobalAppSettings> _globals = winrt::make_self<implementation::GlobalAppSettings>();
        winrt::com_ptr<implementation::Profile> _baseLayerProfile = winrt::make_self<implementation::Profile>();
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> _allProfiles = winrt::single_threaded_observable_vector<Model::Profile>();
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> _activeProfiles = winrt::single_threaded_observable_vector<Model::Profile>();
        winrt::Windows::Foundation::Collections::IVector<Model::ExtensionPackage> _extensionPackages = nullptr;
        std::set<std::string> _themesChangeLog{};

        // load errors
        winrt::Windows::Foundation::Collections::IVector<Model::SettingsLoadWarnings> _warnings = winrt::single_threaded_vector<Model::SettingsLoadWarnings>();
        winrt::Windows::Foundation::IReference<Model::SettingsLoadErrors> _loadError;
        winrt::hstring _deserializationErrorMessage;
        bool _foundInvalidUserResources{ false };

        // defterm
        winrt::Windows::Foundation::Collections::IObservableVector<Model::DefaultTerminal> _defaultTerminals{ nullptr };
        Model::DefaultTerminal _currentDefaultTerminal{ nullptr };

        // GetProfileForArgs cache
        mutable std::once_flag _commandLinesCacheOnce;
        mutable std::vector<std::pair<std::wstring, Model::Profile>> _commandLinesCache;
    };

    struct FragmentProfileEntry : FragmentProfileEntryT<FragmentProfileEntry>
    {
    public:
        FragmentProfileEntry(winrt::guid profileGuid, hstring json) :
            _profileGuid{ profileGuid },
            _json{ json } {}

        winrt::guid ProfileGuid() const noexcept { return _profileGuid; }
        hstring Json() const noexcept { return _json; }

    private:
        winrt::guid _profileGuid;
        hstring _json;
    };

    struct FragmentColorSchemeEntry : FragmentColorSchemeEntryT<FragmentColorSchemeEntry>
    {
    public:
        FragmentColorSchemeEntry(hstring schemeName, hstring json) :
            _schemeName{ schemeName },
            _json{ json } {}

        hstring ColorSchemeName() const noexcept { return _schemeName; }
        hstring Json() const noexcept { return _json; }

    private:
        hstring _schemeName;
        hstring _json;
    };

    struct FragmentSettings : FragmentSettingsT<FragmentSettings>
    {
    public:
        FragmentSettings(hstring source, hstring json, hstring filename) :
            _source{ source },
            _json{ json },
            _filename{ filename } {}

        hstring Source() const noexcept { return _source; }
        hstring Json() const noexcept { return _json; }
        hstring Filename() const noexcept { return _filename; }
        Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry> ModifiedProfiles() const noexcept { return _modifiedProfiles; }
        void ModifiedProfiles(const Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry>& modifiedProfiles) noexcept { _modifiedProfiles = modifiedProfiles; }
        Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry> NewProfiles() const noexcept { return _newProfiles; }
        void NewProfiles(const Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry>& newProfiles) noexcept { _newProfiles = newProfiles; }
        Windows::Foundation::Collections::IVector<Model::FragmentColorSchemeEntry> ColorSchemes() const noexcept { return _colorSchemes; }
        void ColorSchemes(const Windows::Foundation::Collections::IVector<Model::FragmentColorSchemeEntry>& colorSchemes) noexcept { _colorSchemes = colorSchemes; }

        // views
        Windows::Foundation::Collections::IVectorView<Model::FragmentProfileEntry> ModifiedProfilesView() const noexcept { return _modifiedProfiles ? _modifiedProfiles.GetView() : nullptr; }
        Windows::Foundation::Collections::IVectorView<Model::FragmentProfileEntry> NewProfilesView() const noexcept { return _newProfiles ? _newProfiles.GetView() : nullptr; }
        Windows::Foundation::Collections::IVectorView<Model::FragmentColorSchemeEntry> ColorSchemesView() const noexcept { return _colorSchemes ? _colorSchemes.GetView() : nullptr; }

    private:
        hstring _source;
        hstring _json;
        hstring _filename;
        Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry> _modifiedProfiles;
        Windows::Foundation::Collections::IVector<Model::FragmentProfileEntry> _newProfiles;
        Windows::Foundation::Collections::IVector<Model::FragmentColorSchemeEntry> _colorSchemes;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(CascadiaSettings);
}
