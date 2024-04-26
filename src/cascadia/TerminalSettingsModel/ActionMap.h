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
        Control::KeyChord GetKeyBindingForAction(winrt::hstring cmdID) const;

        // population
        void AddAction(const Model::Command& cmd);

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
        std::optional<Model::Command> _GetActionByID(const winrt::hstring actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(const Control::KeyChord& keys) const;

        void _RefreshKeyBindingCaches();
        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateKeyBindingMapWithStandardCommands(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const;

        std::vector<Model::Command> _GetCumulativeActions() const noexcept;

        void _TryUpdateActionMap(const Model::Command& cmd);
        void _TryUpdateKeyChord(const Model::Command& cmd);

        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionsCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _KeyBindingMapCache{ nullptr };

        Windows::Foundation::Collections::IVector<Model::Command> _ExpandedCommandsCache{ nullptr };

        std::unordered_map<winrt::hstring, Model::Command> _NestedCommands;
        std::vector<Model::Command> _IterableCommands;

        bool _fixUpsAppliedDuringLoad;

        void _AddKeyBindingHelper(const Json::Value& json, std::vector<SettingsLoadWarnings>& warnings);
        std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<winrt::hstring, Model::Command> _ActionMap;

        friend class SettingsModelUnitTests::KeyBindingsTests;
        friend class SettingsModelUnitTests::DeserializationTests;
        friend class SettingsModelUnitTests::TerminalSettingsTests;
    };
}
