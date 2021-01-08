/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KeyMapping.h

Abstract:
- A mapping of key chords to actions. Includes (de)serialization logic.

Author(s):
- Carlos Zamora - September 2020

--*/

#pragma once

#include "KeyMapping.g.h"
#include "ActionArgs.h"
#include "../inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class KeyBindingsTests;
    class TestUtils;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct KeyChordHash
    {
        std::size_t operator()(const TerminalControl::KeyChord& key) const
        {
            std::hash<int32_t> keyHash;
            std::hash<TerminalControl::KeyModifiers> modifiersHash;
            std::size_t hashedKey = keyHash(key.Vkey());
            std::size_t hashedMods = modifiersHash(key.Modifiers());
            return hashedKey ^ hashedMods;
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const TerminalControl::KeyChord& lhs, const TerminalControl::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct KeyMapping : KeyMappingT<KeyMapping>
    {
        KeyMapping() = default;
        com_ptr<KeyMapping> Copy() const;

        Model::ActionAndArgs TryLookup(TerminalControl::KeyChord const& chord) const;
        uint64_t Size() const;

        void SetKeyBinding(Model::ActionAndArgs const& actionAndArgs,
                           TerminalControl::KeyChord const& chord);
        void ClearKeyBinding(TerminalControl::KeyChord const& chord);
        TerminalControl::KeyChord GetKeyBindingForAction(Model::ShortcutAction const& action);
        TerminalControl::KeyChord GetKeyBindingForActionWithArgs(Model::ActionAndArgs const& actionAndArgs);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(TerminalControl::KeyModifiers modifiers);

        // Defined in KeyMappingSerialization.cpp
        std::vector<Model::SettingsLoadWarnings> LayerJson(const Json::Value& json);
        Json::Value ToJson();

    private:
        std::unordered_map<TerminalControl::KeyChord, Model::ActionAndArgs, KeyChordHash, KeyChordEquality> _keyShortcuts;
        std::vector<std::pair<TerminalControl::KeyChord, Model::ActionAndArgs>> _keyShortcutsByInsertionOrder;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::TestUtils;
    };
}
