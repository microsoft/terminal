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
#include "ActionArgs.h"
#include "Command.h"
#include "../inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class KeyBindingsTests;
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

    struct ActionMap : ActionMapT<ActionMap>, IInheritable<ActionMap>
    {
        ActionMap() = default;

        // capacity
        size_t KeybindingCount() const noexcept;
        size_t CommandCount() const noexcept;

        // views
        Windows::Foundation::Collections::IMapView<hstring, Model::Command> NameMap() const;
        com_ptr<ActionMap> Copy() const;

        // queries
        Model::Command GetActionByName(hstring const& name) const;
        Model::Command GetActionByKeyChord(Control::KeyChord const& keys) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action) const;
        Control::KeyChord GetKeyBindingForAction(ShortcutAction const& action, IActionArgs const& actionArgs) const;

        // population
        void AddAction(const Model::Command& cmd);
        std::vector<SettingsLoadWarnings> LayerJson(const Json::Value& json);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(Control::KeyModifiers modifiers);

    private:
        Model::Command _GetActionByID(size_t actionID) const;
        bool _IsActionIdValid(size_t actionID) const;

        std::optional<Model::Command> _GetActionByNameInternal(hstring const& name) const;
        std::optional<Model::Command> _GetActionByKeyChordInternal(Control::KeyChord const& keys) const;

        std::unordered_map<hstring, size_t> _NameMap;
        std::unordered_map<Control::KeyChord, size_t, KeyChordHash, KeyChordEquality> _KeyMap;
        std::vector<Model::Command> _ActionList;

        friend class SettingsModelLocalTests::KeyBindingsTests;
    };
}
