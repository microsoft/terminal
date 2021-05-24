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
#include "../inc/cppwinrt_utils.h"

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
        std::size_t operator()(const Control::KeyChord& key) const
        {
            return ::Microsoft::Terminal::Settings::Model::HashUtils::HashProperty(key.Modifiers(), key.Vkey());
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const Control::KeyChord& lhs, const Control::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        ActionMap();

        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> GlobalHotkeys();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> KeyBindings();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(Control::KeyChord const& keys) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action, IActionArgs const& actionArgs) const;

        // population
        void AddAction(const Model::Command& cmd);

        // JSON
        static com_ptr<ActionMap> FromJson(const Json::Value& json);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json);
        Json::Value ToJson() const;

        // modification
        bool RebindKeys(Control::KeyChord const& oldKeys, Control::KeyChord const& newKeys);
        void DeleteKeyBinding(Control::KeyChord const& keys);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(Control::KeyModifiers modifiers);

    private:
        std::optional<Model::Command> _GetActionByID(const InternalActionID actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(Control::KeyChord const& keys) const;

        void _PopulateNameMapWithNestedCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateKeyBindingMapWithStandardCommands(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const;
        std::vector<Model::Command> _GetCumulativeActions() const noexcept;

        void _TryUpdateActionMap(const Model::Command& cmd, Model::Command& oldCmd, Model::Command& consolidatedCmd);
        void _TryUpdateName(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);
        void _TryUpdateKeyChord(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);

        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _KeyBindingMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NestedCommands{ nullptr };
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

        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}
