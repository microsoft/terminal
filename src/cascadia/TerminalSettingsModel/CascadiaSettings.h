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

#include "GlobalAppSettings.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class IDynamicProfileGenerator;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    winrt::com_ptr<Profile> CreateChild(const winrt::com_ptr<Profile>& parent);

    class SettingsTypedDeserializationException final : public std::runtime_error
    {
    public:
        SettingsTypedDeserializationException(const char* message) noexcept :
            std::runtime_error(message) {}
    };

    struct ParsedSettings
    {
        winrt::com_ptr<implementation::GlobalAppSettings> globals;
        winrt::com_ptr<implementation::Profile> baseLayerProfile;
        std::vector<winrt::com_ptr<implementation::Profile>> profiles;
        std::unordered_map<winrt::guid, winrt::com_ptr<implementation::Profile>> profilesByGuid;

        void clear();
    };

    struct SettingsLoader
    {
        static SettingsLoader Default(const std::string_view& userJSON, const std::string_view& inboxJSON);
        SettingsLoader(const std::string_view& userJSON, const std::string_view& inboxJSON);

        void GenerateProfiles();
        void ApplyRuntimeInitialSettings();
        void MergeInboxIntoUserSettings();
        void FindFragmentsAndMergeIntoUserSettings();
        void MergeFragmentIntoUserSettings(const winrt::hstring& source, const std::string_view& content);
        void FinalizeLayering();
        bool DisableDeletedProfiles();
        bool FixupUserSettings();

        ParsedSettings inboxSettings;
        ParsedSettings userSettings;
        bool duplicateProfile = false;

    private:
        struct JsonSettings
        {
            Json::Value root;
            const Json::Value& colorSchemes;
            const Json::Value& profileDefaults;
            const Json::Value& profilesList;
            const Json::Value& themes;
        };

        static std::pair<size_t, size_t> _lineAndColumnFromPosition(const std::string_view& string, const size_t position);
        static void _rethrowSerializationExceptionWithLocationInfo(const JsonUtils::DeserializationError& e, const std::string_view& settingsString);
        static Json::Value _parseJSON(const std::string_view& content);
        static const Json::Value& _getJSONValue(const Json::Value& json, const std::string_view& key) noexcept;
        std::span<const winrt::com_ptr<implementation::Profile>> _getNonUserOriginProfiles() const;
        void _parse(const OriginTag origin, const winrt::hstring& source, const std::string_view& content, ParsedSettings& settings);
        void _parseFragment(const winrt::hstring& source, const std::string_view& content, ParsedSettings& settings);
        static JsonSettings _parseJson(const std::string_view& content);
        static winrt::com_ptr<implementation::Profile> _parseProfile(const OriginTag origin, const winrt::hstring& source, const Json::Value& profileJson);
        void _appendProfile(winrt::com_ptr<Profile>&& profile, const winrt::guid& guid, ParsedSettings& settings);
        void _addUserProfileParent(const winrt::com_ptr<implementation::Profile>& profile);
        void _executeGenerator(const IDynamicProfileGenerator& generator);

        std::unordered_set<std::wstring_view> _ignoredNamespaces;
        // See _getNonUserOriginProfiles().
        size_t _userProfileCount = 0;
    };

    struct CascadiaSettings : CascadiaSettingsT<CascadiaSettings>
    {
    public:
        static MTSM::CascadiaSettings LoadDefaults();
        static MTSM::CascadiaSettings LoadAll();
        static MTSM::CascadiaSettings LoadUniversal();

        static winrt::hstring SettingsPath();
        static winrt::hstring DefaultSettingsPath();
        static winrt::hstring ApplicationDisplayName();
        static winrt::hstring ApplicationVersion();
        static void ExportFile(winrt::hstring path, winrt::hstring content);

        CascadiaSettings() noexcept = default;
        CascadiaSettings(const winrt::hstring& userJSON, const winrt::hstring& inboxJSON);
        CascadiaSettings(const std::string_view& userJSON, const std::string_view& inboxJSON = {});
        explicit CascadiaSettings(SettingsLoader&& loader);

        // user settings
        winrt::hstring Hash() const noexcept;
        MTSM::CascadiaSettings Copy() const;
        MTSM::GlobalAppSettings GlobalSettings() const;
        WFC::IObservableVector<MTSM::Profile> AllProfiles() const noexcept;
        WFC::IObservableVector<MTSM::Profile> ActiveProfiles() const noexcept;
        MTSM::ActionMap ActionMap() const noexcept;
        void WriteSettingsToDisk();
        Json::Value ToJson() const;
        MTSM::Profile ProfileDefaults() const;
        MTSM::Profile CreateNewProfile();
        MTSM::Profile FindProfile(const winrt::guid& guid) const noexcept;
        void UpdateColorSchemeReferences(const winrt::hstring& oldName, const winrt::hstring& newName);
        MTSM::Profile GetProfileForArgs(const MTSM::NewTerminalArgs& newTerminalArgs) const;
        MTSM::Profile GetProfileByName(const winrt::hstring& name) const;
        MTSM::Profile GetProfileByIndex(uint32_t index) const;
        MTSM::Profile DuplicateProfile(const MTSM::Profile& source);

        // load errors
        WFC::IVectorView<Model::SettingsLoadWarnings> Warnings() const;
        WF::IReference<Model::SettingsLoadErrors> GetLoadingError() const;
        winrt::hstring GetSerializationErrorMessage() const;

        // defterm
        static std::wstring NormalizeCommandLine(LPCWSTR commandLine);
        static bool IsDefaultTerminalAvailable() noexcept;
        static bool IsDefaultTerminalSet() noexcept;
        WFC::IObservableVector<MTSM::DefaultTerminal> DefaultTerminals() noexcept;
        MTSM::DefaultTerminal CurrentDefaultTerminal() noexcept;
        void CurrentDefaultTerminal(const MTSM::DefaultTerminal& terminal);

    private:
        static const std::filesystem::path& _settingsPath();
        static const std::filesystem::path& _releaseSettingsPath();
        static winrt::hstring _calculateHash(std::string_view settings, const FILETIME& lastWriteTime);

        winrt::com_ptr<implementation::Profile> _createNewProfile(const std::wstring_view& name) const;
        MTSM::Profile _getProfileForCommandLine(const winrt::hstring& commandLine) const;
        void _refreshDefaultTerminals();

        void _resolveDefaultProfile() const;
        void _resolveNewTabMenuProfiles() const;
        void _resolveNewTabMenuProfilesSet(const WFC::IVector<MTSM::NewTabMenuEntry> entries, WFC::IMap<int, MTSM::Profile>& remainingProfiles, MTSM::RemainingProfilesEntry& remainingProfilesEntry) const;

        void _validateSettings();
        void _validateAllSchemesExist();
        void _validateMediaResources();
        void _validateKeybindings() const;
        void _validateColorSchemesInCommands() const;
        bool _hasInvalidColorScheme(const MTSM::Command& command) const;
        void _validateThemeExists();

        void _researchOnLoad();

        // user settings
        winrt::hstring _hash;
        winrt::com_ptr<implementation::GlobalAppSettings> _globals = winrt::make_self<implementation::GlobalAppSettings>();
        winrt::com_ptr<implementation::Profile> _baseLayerProfile = winrt::make_self<implementation::Profile>();
        WFC::IObservableVector<MTSM::Profile> _allProfiles = winrt::single_threaded_observable_vector<MTSM::Profile>();
        WFC::IObservableVector<MTSM::Profile> _activeProfiles = winrt::single_threaded_observable_vector<MTSM::Profile>();

        // load errors
        WFC::IVector<Model::SettingsLoadWarnings> _warnings = winrt::single_threaded_vector<Model::SettingsLoadWarnings>();
        WF::IReference<Model::SettingsLoadErrors> _loadError;
        winrt::hstring _deserializationErrorMessage;

        // defterm
        WFC::IObservableVector<MTSM::DefaultTerminal> _defaultTerminals{ nullptr };
        MTSM::DefaultTerminal _currentDefaultTerminal{ nullptr };

        // GetProfileForArgs cache
        mutable std::once_flag _commandLinesCacheOnce;
        mutable std::vector<std::pair<std::wstring, MTSM::Profile>> _commandLinesCache;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(CascadiaSettings);
}
