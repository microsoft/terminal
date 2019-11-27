// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "ActionArgs.h"
#include "ShortcutActionDispatch.h"
#include "..\inc\cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class KeyBindingsTests;
}

namespace winrt::TerminalApp::implementation
{
    struct KeyChordHash
    {
        std::size_t operator()(const winrt::Microsoft::Terminal::Settings::KeyChord& key) const
        {
            std::hash<int32_t> keyHash;
            std::hash<winrt::Microsoft::Terminal::Settings::KeyModifiers> modifiersHash;
            std::size_t hashedKey = keyHash(key.Vkey());
            std::size_t hashedMods = modifiersHash(key.Modifiers());
            return hashedKey ^ hashedMods;
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const winrt::Microsoft::Terminal::Settings::KeyChord& lhs, const winrt::Microsoft::Terminal::Settings::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(winrt::Microsoft::Terminal::Settings::KeyChord const& kc);

        void SetKeyBinding(TerminalApp::ActionAndArgs const& actionAndArgs,
                           winrt::Microsoft::Terminal::Settings::KeyChord const& chord);
        void ClearKeyBinding(winrt::Microsoft::Terminal::Settings::KeyChord const& chord);
        Microsoft::Terminal::Settings::KeyChord GetKeyBindingForAction(TerminalApp::ShortcutAction const& action);
        Microsoft::Terminal::Settings::KeyChord GetKeyBindingForActionWithArgs(TerminalApp::ActionAndArgs const& actionAndArgs);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(winrt::Microsoft::Terminal::Settings::KeyModifiers modifiers);

        // Defined in AppKeyBindingsSerialization.cpp
        void LayerJson(const Json::Value& json);
        Json::Value ToJson();

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

    private:
        std::unordered_map<winrt::Microsoft::Terminal::Settings::KeyChord, TerminalApp::ActionAndArgs, KeyChordHash, KeyChordEquality> _keyShortcuts;

        winrt::TerminalApp::ShortcutActionDispatch _dispatch{ nullptr };

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::KeyBindingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppKeyBindings);
}
