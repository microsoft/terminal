// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindingsSerialization.h"
#include "KeyChordSerialization.h"
#include "Utils.h"
#include "ShortcutActionSerializationKeys.h"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view KeysKey{ "keys" };
static constexpr std::string_view CommandKey{ "command" };

// Function Description:
// - Small helper to create a json value serialization of a single
//   KeyBinding->Action maping. The created object is of schema:
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
Json::Value AppKeyBindingsSerialization::ToJson(const winrt::TerminalApp::AppKeyBindings& bindings)
{
    Json::Value bindingsArray;

    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : commandNames)
    {
        const auto searchedForName = actionName.first;
        const auto searchedForAction = actionName.second;

        if (const auto chord{ bindings.GetKeyBinding(searchedForAction) })
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
//   listed in `commandNames`, and `keys` is an array of keypresses. Currently,
//   the array should contain a single string, which can be deserialized into a
//   KeyChord.
// Arguments:
// - json: and array of JsonObject's to deserialize into our _keyShortcuts mapping.
// Return Value:
// - the newly constructed AppKeyBindings object.
winrt::TerminalApp::AppKeyBindings AppKeyBindingsSerialization::FromJson(const Json::Value& json)
{
    winrt::TerminalApp::AppKeyBindings newBindings{};

    for (const auto& value : json)
    {
        if (value.isObject())
        {
            const auto commandString = value[JsonKey(CommandKey)];
            const auto keys = value[JsonKey(KeysKey)];

            if (commandString && keys)
            {
                if (!keys.isArray() || keys.size() != 1)
                {
                    continue;
                }
                const auto keyChordString = winrt::to_hstring(keys[0].asString());
                ShortcutAction action;

                // Try matching the command to one we have
                const auto found = commandNames.find(commandString.asString());
                if (found != commandNames.end())
                {
                    action = found->second;
                }
                else
                {
                    continue;
                }

                // Try parsing the chord
                try
                {
                    const auto chord = KeyChordSerialization::FromString(keyChordString);
                    newBindings.SetKeyBinding(action, chord);
                }
                catch (...)
                {
                    continue;
                }
            }
        }
    }
    return newBindings;
}
