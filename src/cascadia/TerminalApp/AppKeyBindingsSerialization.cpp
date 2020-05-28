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

    // TODO: Fix later
    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : ActionAndArgs::ActionNamesMap)
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

// Method Description:
// - Deserialize an AppKeyBindings from the key mappings that are in the array
//   `json`. The json array should contain an array of objects with both a
//   `command` string and a `keys` array, where `command` is one of the names
//   listed in `ActionAndArgs::ActionNamesMap`, and `keys` is an array of
//   keypresses. Currently, the array should contain a single string, which can
//   be deserialized into a KeyChord.
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
            // ShortcutAction action = ShortcutAction::Invalid;

            // // Keybindings can be serialized in two styles:
            // // { "command": "switchToTab0", "keys": ["ctrl+1"] },
            // // { "command": { "action": "switchToTab", "index": 0 }, "keys": ["ctrl+alt+1"] },

            // // 1. In the first case, the "command" is a string, that's the
            // //    action name. There are no provided args, so we'll pass
            // //    Json::Value::null to the parse function.
            // // 2. In the second case, the "command" is an object. We'll use the
            // //    "action" in that object as the action name. We'll then pass
            // //    the "command" object to the arg parser, for further parsing.

            // auto argsVal = Json::Value::null;

            // // Only try to parse the action if it's actually a string value.
            // // `null` will not pass this check.
            // if (commandVal.isString())
            // {
            //     auto commandString = commandVal.asString();
            //     action = GetActionFromString(commandString);
            // }
            // else if (commandVal.isObject())
            // {
            //     const auto actionVal = commandVal[JsonKey(ActionKey)];
            //     if (actionVal.isString())
            //     {
            //         auto actionString = actionVal.asString();
            //         action = GetActionFromString(actionString);
            //         argsVal = commandVal;
            //     }
            // }

            // // Some keybindings can accept other arbitrary arguments. If it
            // // does, we'll try to deserialize any "args" that were provided with
            // // the binding.
            // IActionArgs args{ nullptr };
            // std::vector<::TerminalApp::SettingsLoadWarnings> parseWarnings;
            // const auto deserializersIter = argParsers.find(action);
            // if (deserializersIter != argParsers.end())
            // {
            //     auto pfn = deserializersIter->second;
            //     if (pfn)
            //     {
            //         std::tie(args, parseWarnings) = pfn(argsVal);
            //     }
            //     warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());

            //     // if an arg parser was registered, but failed, bail
            //     if (pfn && args == nullptr)
            //     {
            //         continue;
            //     }
            // }
            auto actionAndArgs = ActionAndArgs::FromJson(commandVal, warnings);

            // Try parsing the chord
            try
            {
                const auto chord = KeyChordSerialization::FromString(keyChordString);

                // If we couldn't find the action they want to set the chord to,
                // or the action was `null` or `"unbound"`, just clear out the
                // keybinding. Otherwise, set the keybinding to the action we
                // found.
                // if (action != ShortcutAction::Invalid)
                // {
                //     auto actionAndArgs = winrt::make_self<ActionAndArgs>();
                //     actionAndArgs->Action(action);
                //     actionAndArgs->Args(args);
                //     SetKeyBinding(*actionAndArgs, chord);
                // }
                // else
                // {
                //     ClearKeyBinding(chord);
                // }
                if (actionAndArgs)
                {
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
