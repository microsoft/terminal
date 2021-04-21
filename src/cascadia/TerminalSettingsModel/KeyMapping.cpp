// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyMapping.h"
#include "KeyChordSerialization.h"
#include "ActionAndArgs.h"

#include "KeyMapping.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    winrt::com_ptr<KeyMapping> KeyMapping::Copy() const
    {
        auto keymap{ winrt::make_self<KeyMapping>() };
        std::for_each(_keyShortcuts.begin(), _keyShortcuts.end(), [keymap](auto& kv) {
            const auto actionAndArgsImpl{ winrt::get_self<ActionAndArgs>(kv.second) };
            keymap->_keyShortcuts[kv.first] = *actionAndArgsImpl->Copy();
        });
        return keymap;
    }

    Microsoft::Terminal::Settings::Model::ActionAndArgs KeyMapping::TryLookup(KeyChord const& chord) const
    {
        const auto result = _keyShortcuts.find(chord);
        if (result != _keyShortcuts.end())
        {
            return result->second;
        }
        return nullptr;
    }

    uint64_t KeyMapping::Size() const
    {
        return _keyShortcuts.size();
    }

    void KeyMapping::SetKeyBinding(const Microsoft::Terminal::Settings::Model::ActionAndArgs& actionAndArgs,
                                   const KeyChord& chord)
    {
        // if the chord is already mapped - clear the mapping
        if (_keyShortcuts.find(chord) != _keyShortcuts.end())
        {
            ClearKeyBinding(chord);
        }

        _keyShortcuts[chord] = actionAndArgs;
        _keyShortcutsByInsertionOrder.push_back(std::make_pair(chord, actionAndArgs));
    }

    // Method Description:
    // - Remove the action that's bound to a particular KeyChord.
    // Arguments:
    // - chord: the keystroke to remove the action for.
    // Return Value:
    // - <none>
    void KeyMapping::ClearKeyBinding(const KeyChord& chord)
    {
        _keyShortcuts.erase(chord);

        KeyChordEquality keyChordEquality;
        _keyShortcutsByInsertionOrder.erase(std::remove_if(_keyShortcutsByInsertionOrder.begin(), _keyShortcutsByInsertionOrder.end(), [keyChordEquality, chord](const auto& keyBinding) {
                                                return keyChordEquality(keyBinding.first, chord);
                                            }),
                                            _keyShortcutsByInsertionOrder.end());
    }

    KeyChord KeyMapping::GetKeyBindingForAction(Microsoft::Terminal::Settings::Model::ShortcutAction const& action)
    {
        for (auto it = _keyShortcutsByInsertionOrder.rbegin(); it != _keyShortcutsByInsertionOrder.rend(); ++it)
        {
            const auto& kv = *it;
            if (kv.second.Action() == action)
            {
                return kv.first;
            }
        }
        return { nullptr };
    }

    // Method Description:
    // - Lookup the keychord bound to a particular combination of ShortcutAction
    //   and IActionArgs. This enables searching no only for the binding of a
    //   particular ShortcutAction, but also a particular set of values for
    //   arguments to that action.
    //   If several bindings might match the lookup, prefers the one that was added last.
    // Arguments:
    // - actionAndArgs: The ActionAndArgs to lookup the keybinding for.
    // Return Value:
    // - The bound keychord, if this ActionAndArgs is bound to a key, otherwise nullptr.
    KeyChord KeyMapping::GetKeyBindingForActionWithArgs(Microsoft::Terminal::Settings::Model::ActionAndArgs const& actionAndArgs)
    {
        if (actionAndArgs == nullptr)
        {
            return { nullptr };
        }

        for (auto it = _keyShortcutsByInsertionOrder.rbegin(); it != _keyShortcutsByInsertionOrder.rend(); ++it)
        {
            const auto& kv = *it;
            const auto action = kv.second.Action();
            const auto args = kv.second.Args();
            const auto actionMatched = action == actionAndArgs.Action();
            const auto argsMatched = args ? args.Equals(actionAndArgs.Args()) : args == actionAndArgs.Args();
            if (actionMatched && argsMatched)
            {
                return kv.first;
            }
        }
        return { nullptr };
    }

    // Method Description:
    // - Takes the KeyModifier flags from Terminal and maps them to the WinRT types which are used by XAML
    // Return Value:
    // - a Windows::System::VirtualKeyModifiers object with the flags of which modifiers used.
    Windows::System::VirtualKeyModifiers KeyMapping::ConvertVKModifiers(KeyModifiers modifiers)
    {
        Windows::System::VirtualKeyModifiers keyModifiers = Windows::System::VirtualKeyModifiers::None;

        if (WI_IsFlagSet(modifiers, KeyModifiers::Ctrl))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Control;
        }
        if (WI_IsFlagSet(modifiers, KeyModifiers::Shift))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Shift;
        }
        if (WI_IsFlagSet(modifiers, KeyModifiers::Alt))
        {
            // note: Menu is the Alt VK_MENU
            keyModifiers |= Windows::System::VirtualKeyModifiers::Menu;
        }
        if (WI_IsFlagSet(modifiers, KeyModifiers::Windows))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Windows;
        }

        return keyModifiers;
    }
}
