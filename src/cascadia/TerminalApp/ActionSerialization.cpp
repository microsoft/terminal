// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionSerialization.h"
#include "KeyChordSerialization.h"
#include "Utils.h"
#include "ShortcutActionSerializationKeys.h"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconPathKey{ "iconPath" };
static constexpr std::string_view CommandKey{ "command" };

// Method Description:
// - TODO Serialize this AppKeyBindings to a json array of objects. Each object in
//   the array represents a single keybinding, mapping a KeyChord to a
//   ShortcutAction.
// Return Value:
// - a Json::Value which is an equivalent serialization of this object.
Json::Value ActionSerialization::ToJson(const winrt::TerminalApp::Action& action)
{
    Json::Value jsonObject;

    jsonObject[JsonKey(NameKey)] = winrt::to_string(action.Name());
    jsonObject[JsonKey(IconPathKey)] = winrt::to_string(action.IconPath());

    // Iterate over all the possible actions in the names list, to find the name for our command
    for (auto& nameAndAction : commandNames)
    {
        const auto searchedForName = nameAndAction.first;
        const auto searchedForAction = nameAndAction.second;
        if (searchedForAction == action.Command())
        {
            // We found the name, we serialized successfully.
            jsonObject[JsonKey(CommandKey)] = searchedForName.data(); //winrt::to_string(action.IconPath());
            return jsonObject;
        }
    }

    // We did not find a name, throw an error
    throw winrt::hresult_invalid_argument();
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
winrt::TerminalApp::Action ActionSerialization::FromJson(const Json::Value& json)
{
    winrt::TerminalApp::Action result{};

    if (auto name{ json[JsonKey(NameKey)] })
    {
        result.Name(winrt::to_hstring(name.asString()));
    }
    if (auto iconPath{ json[JsonKey(IconPathKey)] })
    {
        result.IconPath(winrt::to_hstring(iconPath.asString()));
    }
    if (auto commandString{ json[JsonKey(CommandKey)] })
    {
        // Try matching the command to one we have
        const auto found = commandNames.find(commandString.asString());
        if (found != commandNames.end())
        {
            result.Command(found->second);
        }
        else
        {
            // We did not find a matching name, throw an error
            throw winrt::hresult_invalid_argument();
        }
    }

    return result;
}
