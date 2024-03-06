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

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::ActionAndArgs> AvailableActions();
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> GlobalHotkeys();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> KeyBindings();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(const Control::KeyChord& keys) const;
        bool IsKeyChordExplicitlyUnbound(const Control::KeyChord& keys) const;
        Control::KeyChord GetKeyBindingForAction(const ShortcutAction& action) const;
        Control::KeyChord GetKeyBindingForAction(const ShortcutAction& action, const IActionArgs& actionArgs) const;

        // population
        void AddAction(const Model::Command& cmd);

        // JSON
        static com_ptr<ActionMap> FromJson(const Json::Value& json);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json, const bool withKeybindings = true);
        Json::Value ToJson() const;

        // modification
        bool RebindKeys(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys);
        void DeleteKeyBinding(const Control::KeyChord& keys);
        void RegisterKeyBinding(Control::KeyChord keys, Model::ActionAndArgs action);

        Windows::Foundation::Collections::IVector<Model::Command> ExpandedCommands();
        void ExpandCommands(const Windows::Foundation::Collections::IVectorView<Model::Profile>& profiles,
                            const Windows::Foundation::Collections::IMapView<winrt::hstring, Model::ColorScheme>& schemes);

        winrt::Windows::Foundation::Collections::IVector<Model::Command> FilterToSendInput(winrt::hstring currentCommandline);

    private:
        std::optional<Model::Command> _GetActionByID(const InternalActionID actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(const Control::KeyChord& keys) const;

        void _RefreshKeyBindingCaches();
        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateKeyBindingMapWithStandardCommands(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const;
        std::vector<Model::Command> _GetCumulativeActions() const noexcept;

        void _TryUpdateActionMap(const Model::Command& cmd, Model::Command& oldCmd, Model::Command& consolidatedCmd);
        void _TryUpdateName(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);
        void _TryUpdateKeyChord(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);

        void _recursiveUpdateCommandKeybindingLabels();

        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionsCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _KeyBindingMapCache{ nullptr };

        Windows::Foundation::Collections::IVector<Model::Command> _ExpandedCommandsCache{ nullptr };

        std::unordered_map<winrt::hstring, Model::Command> _NestedCommands;
        std::vector<Model::Command> _IterableCommands;
        std::unordered_map<Control::KeyChord, InternalActionID, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<InternalActionID, Model::Command> _ActionMap;

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
        std::unordered_map<InternalActionID, Model::Command> _MaskingActions;

        friend class SettingsModelUnitTests::KeyBindingsTests;
        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::TerminalSettingsTests;
    };
}
