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
    typedef size_t InternalActionID;

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

    struct ActionHash
    {
        InternalActionID operator()(const Model::ActionAndArgs& actionAndArgs) const
        {
            std::hash<Model::ShortcutAction> actionHash;
            std::size_t hashedAction{ actionHash(actionAndArgs.Action()) };

            std::size_t hashedArgs;
            if (const auto& args{ actionAndArgs.Args() })
            {
                hashedArgs = gsl::narrow_cast<size_t>(args.Hash());
            }
            else
            {
                std::hash<IActionArgs> argsHash;
                hashedArgs = argsHash(nullptr);
            }
            return hashedAction ^ hashedArgs;
        }
    };

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        ActionMap();

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
        std::optional<Model::Command> _GetActionByID(InternalActionID actionID) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(Control::KeyChord const& keys) const;

        void _PopulateNameMapWithNestedCommands(std::unordered_map<hstring, Model::Command>& nameMap) const;
        void _PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap, std::set<InternalActionID>& visitedActionIDs) const;

        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NameMapCache{ nullptr };
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _NestedCommands{ nullptr };
        std::unordered_map<Control::KeyChord, InternalActionID, KeyChordHash, KeyChordEquality> _KeyMap;
        std::unordered_map<InternalActionID, Model::Command> _ActionMap;

        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::TerminalSettingsTests;
    };
}
