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
#include "IInheritable.h"
#include "Command.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
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
        inline std::size_t operator()(const MTControl::KeyChord& key) const
        {
            return static_cast<size_t>(key.Hash());
        }
    };

    struct KeyChordEquality
    {
        inline bool operator()(const MTControl::KeyChord& lhs, const MTControl::KeyChord& rhs) const
        {
            return lhs.Equals(rhs);
        }
    };

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        // views
        WFC::IMapView<hstring, MTSM::ActionAndArgs> AvailableActions();
        WFC::IMapView<hstring, MTSM::Command> NameMap();
        WFC::IMapView<MTControl::KeyChord, MTSM::Command> GlobalHotkeys();
        WFC::IMapView<MTControl::KeyChord, MTSM::Command> KeyBindings();
        com_ptr<ActionMap> Copy() const;

        // queries
        MTSM::Command GetActionByKeyChord(const MTControl::KeyChord& keys) const;
        bool IsKeyChordExplicitlyUnbound(const MTControl::KeyChord& keys) const;
        MTControl::KeyChord GetKeyBindingForAction(const ShortcutAction& action) const;
        MTControl::KeyChord GetKeyBindingForAction(const ShortcutAction& action, const IActionArgs& actionArgs) const;

        // population
        void AddAction(const MTSM::Command& cmd);

        // JSON
        static com_ptr<ActionMap> FromJson(const Json::Value& json);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        // modification
        bool RebindKeys(const MTControl::KeyChord& oldKeys, const MTControl::KeyChord& newKeys);
        void DeleteKeyBinding(const MTControl::KeyChord& keys);
        void RegisterKeyBinding(MTControl::KeyChord keys, MTSM::ActionAndArgs action);

    private:
        std::optional<MTSM::Command> _GetActionByID(const InternalActionID actionID) const;
        std::optional<MTSM::Command> _GetActionByKeyChordInternal(const MTControl::KeyChord& keys) const;

        void _RefreshKeyBindingCaches();
        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, MTSM::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, MTSM::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, MTSM::Command>& nameMap) const;
        void _PopulateKeyBindingMapWithStandardCommands(std::unordered_map<MTControl::KeyChord, MTSM::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<MTControl::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const;
        std::vector<MTSM::Command> _GetCumulativeActions() const noexcept;

        void _TryUpdateActionMap(const MTSM::Command& cmd, MTSM::Command& oldCmd, MTSM::Command& consolidatedCmd);
        void _TryUpdateName(const MTSM::Command& cmd, const MTSM::Command& oldCmd, const MTSM::Command& consolidatedCmd);
        void _TryUpdateKeyChord(const MTSM::Command& cmd, const MTSM::Command& oldCmd, const MTSM::Command& consolidatedCmd);

        WFC::IMap<hstring, MTSM::ActionAndArgs> _AvailableActionsCache{ nullptr };
        WFC::IMap<hstring, MTSM::Command> _NameMapCache{ nullptr };
        WFC::IMap<MTControl::KeyChord, MTSM::Command> _GlobalHotkeysCache{ nullptr };
        WFC::IMap<MTControl::KeyChord, MTSM::Command> _KeyBindingMapCache{ nullptr };

        std::unordered_map<winrt::hstring, MTSM::Command> _NestedCommands;
        std::vector<MTSM::Command> _IterableCommands;
        std::unordered_map<MTControl::KeyChord, InternalActionID, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<InternalActionID, MTSM::Command> _ActionMap;

        // Masking Actions:
        // These are actions that were introduced in an ancestor,
        //   but were edited (or unbound) in the current layer.
        // _ActionMap shows a Command with keys that were added in this layer,
        //   whereas _MaskingActions provides a view that encompasses all of
        //   the valid associated key chords.
        // Maintaining this map allows us to return a valid Command
        //   in GetKeyBindingForAction.
        // Additionally, these commands to not need to be serialized,
        //   whereas those in _ActionMap do. These actions provide more data
        //   than is necessary to be serialized.
        std::unordered_map<InternalActionID, MTSM::Command> _MaskingActions;

        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}
