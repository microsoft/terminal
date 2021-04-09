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
    struct KeyChordHash
    {
        std::size_t operator()(const Control::KeyChord& key) const
        {
            std::hash<int32_t> keyHash;
            std::hash<Control::KeyModifiers> modifiersHash;
            std::size_t hashedKey = keyHash(key.Vkey());
            std::size_t hashedMods = modifiersHash(key.Modifiers());
            return hashedKey ^ hashedMods;
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const Control::KeyChord& lhs, const Control::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct ActionHash
    {
        std::size_t operator()(const Model::ActionAndArgs& actionAndArgs) const
        {
            std::hash<Model::ShortcutAction> actionHash;
            std::size_t hashedAction{ actionHash(actionAndArgs.Action()) };

            std::hash<IActionArgs> argsHash;
            std::size_t hashedArgs{ argsHash(nullptr) };
            if (const auto& args{ actionAndArgs.Args() })
            {
                hashedArgs = args.Hash();
            }
            return hashedAction ^ hashedArgs;
        }
    };

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        ActionMap() = default;

        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap();
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByKeyChord(Control::KeyChord const& keys) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action, IActionArgs const& actionArgs) const;

        // population
        void AddAction(const Model::Command& cmd);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(Control::KeyModifiers modifiers);

    private:
        std::optional<Model::Command> _GetActionByID(size_t actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(Control::KeyChord const& keys) const;

        void _PopulateNameMap(std::unordered_map<hstring, Model::Command>& nameMap, std::set<size_t>& visitedActionIDs) const;

        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        std::unordered_map<Control::KeyChord, size_t, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<size_t, Model::Command> _ActionMap;

        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}
