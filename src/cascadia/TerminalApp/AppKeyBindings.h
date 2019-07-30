// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "..\inc\cppwinrt_utils.h"

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
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        bool TryKeyChord(winrt::Microsoft::Terminal::Settings::KeyChord const& kc);
        void SetKeyBinding(TerminalApp::ShortcutAction const& action, winrt::Microsoft::Terminal::Settings::KeyChord const& chord);
        Microsoft::Terminal::Settings::KeyChord GetKeyBinding(TerminalApp::ShortcutAction const& action);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(winrt::Microsoft::Terminal::Settings::KeyModifiers modifiers);

    private:
        std::unordered_map<winrt::Microsoft::Terminal::Settings::KeyChord, TerminalApp::ShortcutAction, KeyChordHash, KeyChordEquality> _keyShortcuts;
        winrt::TerminalApp::ShortcutActionDispatch _dispatch{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings, implementation::AppKeyBindings>
    {
    };
}
