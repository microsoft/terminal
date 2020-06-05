// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// - A couple helper functions for serializing/deserializing an AppKeyBindings
//   to/from json.
//
// Author(s):
// - Mike Griese - May 2019

#include "pch.h"
#include "AppKeyBindings.h"
#include "ActionAndArgs.h"
#include "KeyChordSerialization.h"
#include "Utils.h"
#include "JsonUtils.h"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view KeysKey{ "keys" };
static constexpr std::string_view CommandKey{ "command" };
static constexpr std::string_view ActionKey{ "action" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

static constexpr std::string_view CopyTextKey{ "copy" };
static constexpr std::string_view PasteTextKey{ "paste" };
static constexpr std::string_view OpenNewTabDropdownKey{ "openNewTabDropdown" };
static constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
static constexpr std::string_view NewTabKey{ "newTab" };
static constexpr std::string_view NewWindowKey{ "newWindow" };
static constexpr std::string_view CloseWindowKey{ "closeWindow" };
static constexpr std::string_view CloseTabKey{ "closeTab" };
static constexpr std::string_view ClosePaneKey{ "closePane" };
static constexpr std::string_view SwitchtoTabKey{ "switchToTab" };
static constexpr std::string_view NextTabKey{ "nextTab" };
static constexpr std::string_view PrevTabKey{ "prevTab" };
static constexpr std::string_view AdjustFontSizeKey{ "adjustFontSize" };
static constexpr std::string_view ResetFontSizeKey{ "resetFontSize" };
static constexpr std::string_view ScrollupKey{ "scrollUp" };
static constexpr std::string_view ScrolldownKey{ "scrollDown" };
static constexpr std::string_view ScrolluppageKey{ "scrollUpPage" };
static constexpr std::string_view ScrolldownpageKey{ "scrollDownPage" };
static constexpr std::string_view SwitchToTabKey{ "switchToTab" };
static constexpr std::string_view OpenSettingsKey{ "openSettings" }; // TODO GH#2557: Add args for OpenSettings
static constexpr std::string_view SplitPaneKey{ "splitPane" };
static constexpr std::string_view ResizePaneKey{ "resizePane" };
static constexpr std::string_view MoveFocusKey{ "moveFocus" };
static constexpr std::string_view FindKey{ "find" };
static constexpr std::string_view ToggleFullscreenKey{ "toggleFullscreen" };

// Specifically use a map here over an unordered_map. We want to be able to
// iterate over these entries in-order when we're serializing the keybindings.
// HERE BE DRAGONS:
// These are string_views that are being used as keys. These string_views are
// just pointers to other strings. This could be dangerous, if the map outlived
// the actual strings being pointed to. However, since both these strings and
// the map are all const for the lifetime of the app, we have nothing to worry
// about here.
static const std::map<std::string_view, ShortcutAction, std::less<>> commandNames{
    { CopyTextKey, ShortcutAction::CopyText },
    { PasteTextKey, ShortcutAction::PasteText },
    { OpenNewTabDropdownKey, ShortcutAction::OpenNewTabDropdown },
    { DuplicateTabKey, ShortcutAction::DuplicateTab },
    { NewTabKey, ShortcutAction::NewTab },
    { NewWindowKey, ShortcutAction::NewWindow },
    { CloseWindowKey, ShortcutAction::CloseWindow },
    { CloseTabKey, ShortcutAction::CloseTab },
    { ClosePaneKey, ShortcutAction::ClosePane },
    { NextTabKey, ShortcutAction::NextTab },
    { PrevTabKey, ShortcutAction::PrevTab },
    { AdjustFontSizeKey, ShortcutAction::AdjustFontSize },
    { ResetFontSizeKey, ShortcutAction::ResetFontSize },
    { ScrollupKey, ShortcutAction::ScrollUp },
    { ScrolldownKey, ShortcutAction::ScrollDown },
    { ScrolluppageKey, ShortcutAction::ScrollUpPage },
    { ScrolldownpageKey, ShortcutAction::ScrollDownPage },
    { SwitchToTabKey, ShortcutAction::SwitchToTab },
    { ResizePaneKey, ShortcutAction::ResizePane },
    { MoveFocusKey, ShortcutAction::MoveFocus },
    { OpenSettingsKey, ShortcutAction::OpenSettings },
    { ToggleFullscreenKey, ShortcutAction::ToggleFullscreen },
    { SplitPaneKey, ShortcutAction::SplitPane },
    { UnboundKey, ShortcutAction::Invalid },
    { FindKey, ShortcutAction::Find },
};

using ParseResult = std::tuple<IActionArgs, std::vector<TerminalApp::SettingsLoadWarnings>>;
using ParseActionFunction = std::function<ParseResult(const Json::Value&)>;

// This is a map of ShortcutAction->function<IActionArgs(Json::Value)>. It holds
// a set of deserializer functions that can be used to deserialize a IActionArgs
// from json. Each type of IActionArgs that can accept arbitrary args should be
// placed into this map, with the corresponding deserializer function as the
// value.
static const std::map<ShortcutAction, ParseActionFunction, std::less<>> argParsers{
    { ShortcutAction::CopyText, winrt::TerminalApp::implementation::CopyTextArgs::FromJson },

    { ShortcutAction::NewTab, winrt::TerminalApp::implementation::NewTabArgs::FromJson },

    { ShortcutAction::SwitchToTab, winrt::TerminalApp::implementation::SwitchToTabArgs::FromJson },

    { ShortcutAction::ResizePane, winrt::TerminalApp::implementation::ResizePaneArgs::FromJson },

    { ShortcutAction::MoveFocus, winrt::TerminalApp::implementation::MoveFocusArgs::FromJson },

    { ShortcutAction::AdjustFontSize, winrt::TerminalApp::implementation::AdjustFontSizeArgs::FromJson },

    { ShortcutAction::SplitPane, winrt::TerminalApp::implementation::SplitPaneArgs::FromJson },

    { ShortcutAction::Invalid, nullptr },
};

// Function Description:
// - Small helper to create a json value serialization of a single
//   KeyBinding->Action mapping.
//   {
//      keys:[String],
//      command:String
//   }
// Arguments:
// - chord: The KeyChord to serialize
// - actionName: the name of the ShortcutAction to use with this KeyChord
// Return Value:
// - a Json::Value which is an equivalent serialization of this object.
static Json::Value _ShortcutAsJsonObject(const KeyChord& chord,
                                         const std::string_view actionName)
{
    const auto keyString = KeyChordSerialization::ToString(chord);
    if (keyString == L"")
    {
        return nullptr;
    }

    Json::Value jsonObject;
    Json::Value keysArray;
    keysArray.append(winrt::to_string(keyString));

    jsonObject[JsonKey(KeysKey)] = keysArray;
    jsonObject[JsonKey(CommandKey)] = actionName.data();

    return jsonObject;
}

// Method Description:
// - Serialize this AppKeyBindings to a json array of objects. Each object in
//   the array represents a single keybinding, mapping a KeyChord to a
//   ShortcutAction.
// Return Value:
// - a Json::Value which is an equivalent serialization of this object.
Json::Value winrt::TerminalApp::implementation::AppKeyBindings::ToJson()
{
    Json::Value bindingsArray;

    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : commandNames)
    {
        const auto searchedForName = actionName.first;
        const auto searchedForAction = actionName.second;

        if (const auto chord{ GetKeyBindingForAction(searchedForAction) })
        {
            if (const auto serialization{ _ShortcutAsJsonObject(chord, searchedForName) })
            {
                bindingsArray.append(serialization);
            }
        }
    }

    return bindingsArray;
}

// Function Description:
// - Attempts to match a string to a ShortcutAction. If there's no match, then
//   returns ShortcutAction::Invalid
// Arguments:
// - actionString: the string to match to a ShortcutAction
// Return Value:
// - The ShortcutAction corresponding to the given string, if a match exists.
static ShortcutAction GetActionFromString(const std::string_view actionString)
{
    // Try matching the command to one we have. If we can't find the
    // action name in our list of names, let's just unbind that key.
    const auto found = commandNames.find(actionString);
    return found != commandNames.end() ? found->second : ShortcutAction::Invalid;
}

// Method Description:
// - Deserialize an AppKeyBindings from the key mappings that are in the array
//   `json`. The json array should contain an array of objects with both a
//   `command` string and a `keys` array, where `command` is one of the names
//   listed in `commandNames`, and `keys` is an array of keypresses. Currently,
//   the array should contain a single string, which can be deserialized into a
//   KeyChord.
// - Applies the deserialized keybindings to the provided `bindings` object. If
//   a key chord in `json` is already bound to an action, that chord will be
//   overwritten with the new action. If a chord is bound to `null` or
//   `"unbound"`, then we'll clear the keybinding from the existing keybindings.
// Arguments:
// - json: and array of JsonObject's to deserialize into our _keyShortcuts mapping.
std::vector<::TerminalApp::SettingsLoadWarnings> winrt::TerminalApp::implementation::AppKeyBindings::LayerJson(const Json::Value& json)
{
    // It's possible that the user provided keybindings have some warnings in
    // them - problems that we should alert the user to, but we can recover
    // from. Most of these warnings cannot be detected later in the Validate
    // settings phase, so we'll collect them now.
    std::vector<::TerminalApp::SettingsLoadWarnings> warnings;

    for (const auto& value : json)
    {
        if (!value.isObject())
        {
            continue;
        }

        const auto commandVal = value[JsonKey(CommandKey)];
        const auto keys = value[JsonKey(KeysKey)];

        if (keys)
        {
            const auto validString = keys.isString();
            const auto validArray = keys.isArray() && keys.size() == 1;

            // GH#4239 - If the user provided more than one key
            // chord to a "keys" array, warn the user here.
            // TODO: GH#1334 - remove this check.
            if (keys.isArray() && keys.size() > 1)
            {
                warnings.push_back(::TerminalApp::SettingsLoadWarnings::TooManyKeysForChord);
            }

            if (!validString && !validArray)
            {
                continue;
            }
            const auto keyChordString = keys.isString() ? winrt::to_hstring(keys.asString()) : winrt::to_hstring(keys[0].asString());
            // Invalid is our placeholder that the action was not parsed.
            ShortcutAction action = ShortcutAction::Invalid;

            // Keybindings can be serialized in two styles:
            // { "command": "switchToTab0", "keys": ["ctrl+1"] },
            // { "command": { "action": "switchToTab", "index": 0 }, "keys": ["ctrl+alt+1"] },

            // 1. In the first case, the "command" is a string, that's the
            //    action name. There are no provided args, so we'll pass
            //    Json::Value::null to the parse function.
            // 2. In the second case, the "command" is an object. We'll use the
            //    "action" in that object as the action name. We'll then pass
            //    the "command" object to the arg parser, for further parsing.

            auto argsVal = Json::Value::null;

            // Only try to parse the action if it's actually a string value.
            // `null` will not pass this check.
            if (commandVal.isString())
            {
                auto commandString = commandVal.asString();
                action = GetActionFromString(commandString);
            }
            else if (commandVal.isObject())
            {
                const auto actionVal = commandVal[JsonKey(ActionKey)];
                if (actionVal.isString())
                {
                    auto actionString = actionVal.asString();
                    action = GetActionFromString(actionString);
                    argsVal = commandVal;
                }
            }

            // Some keybindings can accept other arbitrary arguments. If it
            // does, we'll try to deserialize any "args" that were provided with
            // the binding.
            IActionArgs args{ nullptr };
            std::vector<::TerminalApp::SettingsLoadWarnings> parseWarnings;
            const auto deserializersIter = argParsers.find(action);
            if (deserializersIter != argParsers.end())
            {
                auto pfn = deserializersIter->second;
                if (pfn)
                {
                    std::tie(args, parseWarnings) = pfn(argsVal);
                }
                warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());

                // if an arg parser was registered, but failed, bail
                if (pfn && args == nullptr)
                {
                    continue;
                }
            }

            // Try parsing the chord
            try
            {
                const auto chord = KeyChordSerialization::FromString(keyChordString);

                // If we couldn't find the action they want to set the chord to,
                // or the action was `null` or `"unbound"`, just clear out the
                // keybinding. Otherwise, set the keybinding to the action we
                // found.
                if (action != ShortcutAction::Invalid)
                {
                    auto actionAndArgs = winrt::make_self<ActionAndArgs>();
                    actionAndArgs->Action(action);
                    actionAndArgs->Args(args);
                    SetKeyBinding(*actionAndArgs, chord);
                }
                else
                {
                    ClearKeyBinding(chord);
                }
            }
            catch (...)
            {
                continue;
            }
        }
    }

    return warnings;
}
