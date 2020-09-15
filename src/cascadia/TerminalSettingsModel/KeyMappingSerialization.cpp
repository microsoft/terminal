// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// - A couple helper functions for serializing/deserializing a KeyMapping
//   to/from json.
//
// Author(s):
// - Mike Griese - May 2019

#include "pch.h"
#include "KeyMapping.h"
#include "ActionAndArgs.h"
#include "KeyChordSerialization.h"
#include "..\TerminalApp\Utils.h"
#include "JsonUtils.h"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;

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
// - Serialize this KeyMapping to a json array of objects. Each object in
//   the array represents a single keybinding, mapping a KeyChord to a
//   ShortcutAction.
// Return Value:
// - a Json::Value which is an equivalent serialization of this object.
Json::Value winrt::Microsoft::Terminal::Settings::Model::implementation::KeyMapping::ToJson()
{
    Json::Value bindingsArray;

    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : ActionAndArgs::ActionKeyNamesMap)
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
// - Deserialize a KeyMapping from the key mappings that are in the array
//   `json`. The json array should contain an array of objects with both a
//   `command` string and a `keys` array, where `command` is one of the names
//   listed in `ActionAndArgs::ActionKeyNamesMap`, and `keys` is an array of
//   keypresses. Currently, the array should contain a single string, which can
//   be deserialized into a KeyChord.
// - Applies the deserialized keybindings to the provided `bindings` object. If
//   a key chord in `json` is already bound to an action, that chord will be
//   overwritten with the new action. If a chord is bound to `null` or
//   `"unbound"`, then we'll clear the keybinding from the existing keybindings.
// Arguments:
// - json: an array of Json::Value's to deserialize into our _keyShortcuts mapping.
std::vector<SettingsLoadWarnings> winrt::Microsoft::Terminal::Settings::Model::implementation::KeyMapping::LayerJson(const Json::Value& json)
{
    // It's possible that the user provided keybindings have some warnings in
    // them - problems that we should alert the user to, but we can recover
    // from. Most of these warnings cannot be detected later in the Validate
    // settings phase, so we'll collect them now.
    std::vector<SettingsLoadWarnings> warnings;

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
                warnings.push_back(SettingsLoadWarnings::TooManyKeysForChord);
            }

            if (!validString && !validArray)
            {
                continue;
            }
            const auto keyChordString = keys.isString() ? winrt::to_hstring(keys.asString()) : winrt::to_hstring(keys[0].asString());

            // If the action was null, "unbound", or something we didn't
            // understand, this will return nullptr.
            auto actionAndArgs = ActionAndArgs::FromJson(commandVal, warnings);

            // Try parsing the chord
            try
            {
                const auto chord = KeyChordSerialization::FromString(keyChordString);

                // If we couldn't find the action they want to set the chord to,
                // or the action was `null` or `"unbound"`, just clear out the
                // keybinding. Otherwise, set the keybinding to the action we
                // found.
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
