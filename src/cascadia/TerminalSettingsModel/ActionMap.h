/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ActionMap.h

Abstract:
- A mapping of key chords to actions. Includes (de)serialization logic.

Author(s):
- Carlos Zamora - September 2020

--*/

#pragma once

#include "ActionMap.g.h"
#include "ActionArgFactory.g.h"
#include "IInheritable.h"
#include "Command.h"

// fwdecl unittest classes
namespace SettingsModelUnitTests
{
    class KeyBindingsTests;
    class DeserializationTests;
    class TerminalSettingsTests;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    using InternalActionID = size_t;

    struct KeyChordHash
    {
        inline std::size_t operator()(const Control::KeyChord& key) const
        {
            return static_cast<size_t>(key.Hash());
        }
    };

    struct KeyChordEquality
    {
        inline bool operator()(const Control::KeyChord& lhs, const Control::KeyChord& rhs) const
        {
            return lhs.Equals(rhs);
        }
    };

    struct ActionArgFactory
    {
        ActionArgFactory() = default;

        static winrt::hstring GetNameForAction(ShortcutAction action);
        static winrt::hstring GetNameForAction(ShortcutAction action, Windows::ApplicationModel::Resources::Core::ResourceContext context);
        static Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> AvailableShortcutActionsAndNames();
        static Model::IActionArgs GetEmptyArgsForAction(Model::ShortcutAction shortcutAction);
    };

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        void _FinalizeInheritance() override;

        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::ActionAndArgs> AvailableActions();
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> GlobalHotkeys();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> KeyBindings();
        Windows::Foundation::Collections::IVectorView<Model::Command> AllCommands();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(const Control::KeyChord& keys) const;
        Model::Command GetActionByID(const winrt::hstring& cmdID) const;
        bool IsKeyChordExplicitlyUnbound(const Control::KeyChord& keys) const;
        Control::KeyChord GetKeyBindingForAction(const winrt::hstring& cmdID);
        Windows::Foundation::Collections::IVector<Control::KeyChord> AllKeyBindingsForAction(const winrt::hstring& cmdID);

        // population
        void AddAction(const Model::Command& cmd, const Control::KeyChord& keys);

        // JSON
        static com_ptr<ActionMap> FromJson(const Json::Value& json, const OriginTag origin = OriginTag::None);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json, const OriginTag origin, const bool withKeybindings = true);
        Json::Value ToJson() const;
        Json::Value KeyBindingsToJson() const;
        bool FixupsAppliedDuringLoad() const;
        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

        // modification
        bool RebindKeys(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys);
        void DeleteKeyBinding(const Control::KeyChord& keys);
        void AddKeyBinding(Control::KeyChord keys, const winrt::hstring& cmdID);
        void RegisterKeyBinding(Control::KeyChord keys, Model::ActionAndArgs action);
        void DeleteUserCommand(const winrt::hstring& cmdID);
        void AddSendInputAction(winrt::hstring name, winrt::hstring input, const Control::KeyChord keys);
        void UpdateCommandID(const Model::Command& cmd, winrt::hstring newID);

        Windows::Foundation::Collections::IVector<Model::Command> ExpandedCommands();
        void ExpandCommands(const Windows::Foundation::Collections::IVectorView<Model::Profile>& profiles,
                            const Windows::Foundation::Collections::IMapView<winrt::hstring, Model::ColorScheme>& schemes);

        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Foundation::Collections::IVector<Model::Command>> FilterToSnippets(winrt::hstring currentCommandline, winrt::hstring currentWorkingDirectory);

        void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver);

    private:
        Model::Command _GetActionByID(const winrt::hstring& actionID) const;
        std::optional<winrt::hstring> _GetActionIdByKeyChordInternal(const Control::KeyChord& keys) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(const Control::KeyChord& keys) const;

        void _RefreshKeyBindingCaches();
        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;

        void _PopulateCumulativeKeyMaps(std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality>& keyToActionMap, std::unordered_map<winrt::hstring, Control::KeyChord>& actionToKeyMap);
        void _PopulateCumulativeActionMap(std::unordered_map<hstring, Model::Command>& actionMap);

        void _TryUpdateActionMap(const Model::Command& cmd);
        void _TryUpdateKeyChord(const Model::Command& cmd, const Control::KeyChord& keys);

        static std::unordered_map<hstring, Model::Command> _loadLocalSnippets(const std::filesystem::path& currentWorkingDirectory);

        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionsCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };

        Windows::Foundation::Collections::IVector<Model::Command> _ExpandedCommandsCache{ nullptr };

        std::unordered_map<winrt::hstring, Model::Command> _NestedCommands;
        std::vector<Model::Command> _IterableCommands;

        bool _fixupsAppliedDuringLoad{ false };

        // _KeyMap is the map of key chords -> action IDs defined in this layer
        // _ActionMap is the map of action IDs -> commands defined in this layer
        // These maps are the ones that we deserialize into when parsing the user json and vice-versa
        std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<winrt::hstring, Model::Command> _ActionMap;

        // _CumulativeKeyMapCache is the map of key chords -> action IDs defined in all layers, with child layers overriding parent layers
        std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> _CumulativeKeyToActionMapCache;
        // _CumulativeActionMapCache is the map of action IDs -> commands defined in all layers, with child layers overriding parent layers
        std::unordered_map<winrt::hstring, Model::Command> _CumulativeIDToActionMapCache;
        // _CumulativeActionKeyMapCache stores the same data as _CumulativeKeyMapCache, but in the other direction (actionID -> keyChord)
        // This is so we have O(1) lookup time when we want to get the keybinding for a specific action
        // Note that an action could have multiple keybindings, the one we store in this map is one of the user's ones if present,
        // otherwise the default one
        std::unordered_map<winrt::hstring, Control::KeyChord> _CumulativeActionToKeyMapCache;

        // _ResolvedKeyActionMapCache is the map of key chords -> commands defined in all layers, with child layers overriding parent layers
        // This is effectively a combination of _CumulativeKeyMapCache and _CumulativeActionMapCache and its purpose is so that
        // we can give the SUI a view of the key chords and the commands they map to
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _ResolvedKeyToActionMapCache{ nullptr };
        Windows::Foundation::Collections::IVector<Model::Command> _AllCommandsCache{ nullptr };

        til::shared_mutex<std::unordered_map<std::filesystem::path, std::unordered_map<hstring, Model::Command>>> _cwdLocalSnippetsCache{};

        std::set<std::string> _changeLog;

        friend class SettingsModelUnitTests::KeyBindingsTests;
        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::TerminalSettingsTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ActionArgFactory);
}
