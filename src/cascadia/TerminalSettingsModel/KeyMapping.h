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

    struct KeyMapping : KeyMappingT<KeyMapping>
    {
        KeyMapping() = default;
        com_ptr<KeyMapping> Copy() const;

        Model::ActionAndArgs TryLookup(Control::KeyChord const& chord) const;
        uint64_t Size() const;

        void SetKeyBinding(Model::ActionAndArgs const& actionAndArgs,
                           Control::KeyChord const& chord);
        void ClearKeyBinding(Control::KeyChord const& chord);
        Control::KeyChord GetKeyBindingForAction(Model::ShortcutAction const& action);
        Control::KeyChord GetKeyBindingForActionWithArgs(Model::ActionAndArgs const& actionAndArgs);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(Control::KeyModifiers modifiers);

        // Defined in KeyMappingSerialization.cpp
        std::vector<Model::SettingsLoadWarnings> LayerJson(const Json::Value& json);
        Json::Value ToJson();

    private:
        std::unordered_map<Control::KeyChord, Model::ActionAndArgs, KeyChordHash, KeyChordEquality> _keyShortcuts;
        std::vector<std::pair<Control::KeyChord, Model::ActionAndArgs>> _keyShortcutsByInsertionOrder;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::KeyBindingsTests;
        friend class SettingsModelLocalTests::TestUtils;
    };
}
