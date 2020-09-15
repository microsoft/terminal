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
#include "..\inc\cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
    class TestUtils;
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct KeyChordHash
    {
        std::size_t operator()(const winrt::Microsoft::Terminal::TerminalControl::KeyChord& key) const
        {
            std::hash<int32_t> keyHash;
            std::hash<winrt::Microsoft::Terminal::TerminalControl::KeyModifiers> modifiersHash;
            std::size_t hashedKey = keyHash(key.Vkey());
            std::size_t hashedMods = modifiersHash(key.Modifiers());
            return hashedKey ^ hashedMods;
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const winrt::Microsoft::Terminal::TerminalControl::KeyChord& lhs, const winrt::Microsoft::Terminal::TerminalControl::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct KeyMapping : KeyMappingT<KeyMapping>
    {
        KeyMapping() = default;

        Microsoft::Terminal::Settings::Model::ActionAndArgs TryLookup(winrt::Microsoft::Terminal::TerminalControl::KeyChord const& chord) const;

        void SetKeyBinding(Microsoft::Terminal::Settings::Model::ActionAndArgs const& actionAndArgs,
                           winrt::Microsoft::Terminal::TerminalControl::KeyChord const& chord);
        void ClearKeyBinding(winrt::Microsoft::Terminal::TerminalControl::KeyChord const& chord);
        Microsoft::Terminal::TerminalControl::KeyChord GetKeyBindingForAction(Microsoft::Terminal::Settings::Model::ShortcutAction const& action);
        Microsoft::Terminal::TerminalControl::KeyChord GetKeyBindingForActionWithArgs(Microsoft::Terminal::Settings::Model::ActionAndArgs const& actionAndArgs);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(winrt::Microsoft::Terminal::TerminalControl::KeyModifiers modifiers);

        // Defined in KeyMappingSerialization.cpp
        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> LayerJson(const Json::Value& json);
        Json::Value ToJson();

    private:
        std::unordered_map<winrt::Microsoft::Terminal::TerminalControl::KeyChord, Microsoft::Terminal::Settings::Model::ActionAndArgs, KeyChordHash, KeyChordEquality> _keyShortcuts;

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
        friend class TerminalAppLocalTests::TestUtils;
    };
}
