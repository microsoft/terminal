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
    union ActionMapKeyChord
    {
        uint16_t value = 0;
        struct
        {
            uint8_t modifiers;
            uint8_t vkey;
        };

        constexpr ActionMapKeyChord() = default;
        ActionMapKeyChord(const Control::KeyChord& keys) :
            modifiers(gsl::narrow_cast<uint8_t>(keys.Modifiers())),
            vkey(gsl::narrow_cast<uint8_t>(keys.Vkey()))
        {
        }

        constexpr bool operator==(ActionMapKeyChord other) const noexcept
        {
            return value == other.value;
        }
    };
}

template<>
struct std::hash<winrt::Microsoft::Terminal::Settings::Model::implementation::ActionMapKeyChord>
{
    constexpr size_t operator()(winrt::Microsoft::Terminal::Settings::Model::implementation::ActionMapKeyChord keys) const noexcept
    {
        // I didn't like how std::hash uses the byte-wise FNV1a for integers.
        // So I built my own std::hash with murmurhash3.
#if SIZE_MAX == UINT32_MAX
        size_t h = keys.value;
        h ^= h >> 16;
        h *= UINT32_C(0x85ebca6b);
        h ^= h >> 13;
        h *= UINT32_C(0xc2b2ae35);
        h ^= h >> 16;
        return h;
#elif SIZE_MAX == UINT64_MAX
        size_t h = keys.value;
        h ^= h >> 33;
        h *= UINT64_C(0xff51afd7ed558ccd);
        h ^= h >> 33;
        h *= UINT64_C(0xc4ceb9fe1a85ec53);
        h ^= h >> 33;
        return h;
#else
        return std::hash<uint16_t>{}(keys.value);
#endif
    }
};

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
        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::ActionAndArgs> AvailableActions();
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> GlobalHotkeys();
        Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> KeyBindings();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(Control::KeyChord const& keys) const;
        bool IsKeyChordExplicitlyUnbound(Control::KeyChord const& keys) const;
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
        void RegisterKeyBinding(Control::KeyChord keys, Model::ActionAndArgs action);

    private:
        std::optional<Model::Command> _GetActionByID(const InternalActionID actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(const ActionMapKeyChord keys) const;

        void _PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const;
        void _PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateKeyBindingMapWithStandardCommands(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<ActionMapKeyChord>& unboundKeys) const;
        std::vector<Model::Command> _GetCumulativeActions() const noexcept;

        void _TryUpdateActionMap(const Model::Command& cmd, Model::Command& oldCmd, Model::Command& consolidatedCmd);
        void _TryUpdateName(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);
        void _TryUpdateKeyChord(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& consolidatedCmd);

        Windows::Foundation::Collections::IMap<hstring, Model::ActionAndArgs> _AvailableActionsCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _GlobalHotkeysCache{ nullptr };
        Windows::Foundation::Collections::IMap<Control::KeyChord, Model::Command> _KeyBindingMapCache{ nullptr };

        std::unordered_map<winrt::hstring, Model::Command> _NestedCommands;
        std::vector<Model::Command> _IterableCommands;
        std::unordered_map<ActionMapKeyChord, InternalActionID> _KeyMap;
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
