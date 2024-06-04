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
        void _FinalizeInheritance() override;

        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::ActionAndArgs> AvailableActions();
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> GlobalHotkeys();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> KeyBindings();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(const Control::KeyChord& keys) const;
        Model::Command GetActionByID(const winrt::hstring& cmdID) const;
        bool IsKeyChordExplicitlyUnbound(const Control::KeyChord& keys) const;
        Control::KeyChord GetKeyBindingForAction(winrt::hstring cmdID);

        // population
        void AddAction(const Model::Command& cmd, const Control::KeyChord& keys);

        // JSON
        static com_ptr<ActionMap> FromJson(const Json::Value& json, const OriginTag origin = OriginTag::None);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json, const OriginTag origin, const bool withKeybindings = true);
        Json::Value ToJson() const;
        Json::Value KeyBindingsToJson() const;
        bool FixUpsAppliedDuringLoad() const;

        // modification
        bool RebindKeys(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys);
        void DeleteKeyBinding(const Control::KeyChord& keys);
        void RegisterKeyBinding(Control::KeyChord keys, Model::ActionAndArgs action);

        Windows::Foundation::Collections::IVector<Model::Command> ExpandedCommands();
        void ExpandCommands(const Windows::Foundation::Collections::IVectorView<Model::Profile>& profiles,
                            const Windows::Foundation::Collections::IMapView<winrt::hstring, Model::ColorScheme>& schemes);

        winrt::Windows::Foundation::Collections::IVector<Model::Command> FilterToSendInput(winrt::hstring currentCommandline);

    private:
        Model::Command _GetActionByID(const winrt::hstring actionID) const;
        std::optional<winrt::hstring> _GetActionIdByKeyChordInternal(const Control::KeyChord& keys) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(const Control::KeyChord& keys) const;

        void _RefreshKeyBindingCaches();
        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;

        void _PopulateCumulativeKeyMap(std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality>& keyBindingsMap);
        void _PopulateCumulativeActionMap(std::unordered_map<hstring, Model::Command>& actionMap);

        void _TryUpdateActionMap(const Model::Command& cmd);
        void _TryUpdateKeyChord(const Model::Command& cmd, const Control::KeyChord& keys);

        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionsCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };

        Windows::Foundation::Collections::IVector<Model::Command> _ExpandedCommandsCache{ nullptr };

        std::unordered_map<winrt::hstring, Model::Command> _NestedCommands;
        std::vector<Model::Command> _IterableCommands;

        bool _fixUpsAppliedDuringLoad;

        // _KeyMap is the map of key chords -> action IDs defined in this layer
        // _ActionMap is the map of action IDs -> commands defined in this layer
        // These maps are the ones that we deserialize into when parsing the user json and vice-versa
        std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<winrt::hstring, Model::Command> _ActionMap;

        // _CumulativeKeyMapCache is the map of key chords -> action IDs defined in all layers, with child layers overriding parent layers
        Windows::Foundation::Collections::IMap<Control::KeyChord, winrt::hstring> _CumulativeKeyMapCache{ nullptr };
        // _CumulativeActionMapCache is the map of action IDs -> commands defined in all layers, with child layers overriding parent layers
        Windows::Foundation::Collections::IMap<winrt::hstring, Model::Command> _CumulativeActionMapCache{ nullptr };

        // _ResolvedKeyActionMapCache is the map of key chords -> commands defined in all layers, with child layers overriding parent layers
        // This is effectively a combination of _CumulativeKeyMapCache and _CumulativeActionMapCache and its purpose is so that
        // we can give the SUI a view of the key chords and the commands they map to
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _ResolvedKeyActionMapCache{ nullptr };

        friend class SettingsModelUnitTests::KeyBindingsTests;
        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::TerminalSettingsTests;
    };
}
