// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindingsSerialization.h"
#include "KeyChordSerialization.h"
#include "Utils.h"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view KEYS_KEY{ "keys" };
static constexpr std::string_view COMMAND_KEY{ "command" };

static constexpr std::string_view COPYTEXT_KEY{ "copy" };
static constexpr std::string_view PASTETEXT_KEY{ "paste" };
static constexpr std::string_view NEWTAB_KEY{ "newTab" };
static constexpr std::string_view NEWTABWITHPROFILE0_KEY{ "newTabProfile0" };
static constexpr std::string_view NEWTABWITHPROFILE1_KEY{ "newTabProfile1" };
static constexpr std::string_view NEWTABWITHPROFILE2_KEY{ "newTabProfile2" };
static constexpr std::string_view NEWTABWITHPROFILE3_KEY{ "newTabProfile3" };
static constexpr std::string_view NEWTABWITHPROFILE4_KEY{ "newTabProfile4" };
static constexpr std::string_view NEWTABWITHPROFILE5_KEY{ "newTabProfile5" };
static constexpr std::string_view NEWTABWITHPROFILE6_KEY{ "newTabProfile6" };
static constexpr std::string_view NEWTABWITHPROFILE7_KEY{ "newTabProfile7" };
static constexpr std::string_view NEWTABWITHPROFILE8_KEY{ "newTabProfile8" };
static constexpr std::string_view NEWWINDOW_KEY{ "newWindow" };
static constexpr std::string_view CLOSEWINDOW_KEY{ "closeWindow" };
static constexpr std::string_view CLOSETAB_KEY{ "closeTab" };
static constexpr std::string_view SWITCHTOTAB_KEY{ "switchToTab" };
static constexpr std::string_view NEXTTAB_KEY{ "nextTab" };
static constexpr std::string_view PREVTAB_KEY{ "prevTab" };
static constexpr std::string_view INCREASEFONTSIZE_KEY{ "increaseFontSize" };
static constexpr std::string_view DECREASEFONTSIZE_KEY{ "decreaseFontSize" };
static constexpr std::string_view SCROLLUP_KEY{ "scrollUp" };
static constexpr std::string_view SCROLLDOWN_KEY{ "scrollDown" };
static constexpr std::string_view SCROLLUPPAGE_KEY{ "scrollUpPage" };
static constexpr std::string_view SCROLLDOWNPAGE_KEY{ "scrollDownPage" };
static constexpr std::string_view SWITCHTOTAB0_KEY{ "switchToTab0" };
static constexpr std::string_view SWITCHTOTAB1_KEY{ "switchToTab1" };
static constexpr std::string_view SWITCHTOTAB2_KEY{ "switchToTab2" };
static constexpr std::string_view SWITCHTOTAB3_KEY{ "switchToTab3" };
static constexpr std::string_view SWITCHTOTAB4_KEY{ "switchToTab4" };
static constexpr std::string_view SWITCHTOTAB5_KEY{ "switchToTab5" };
static constexpr std::string_view SWITCHTOTAB6_KEY{ "switchToTab6" };
static constexpr std::string_view SWITCHTOTAB7_KEY{ "switchToTab7" };
static constexpr std::string_view SWITCHTOTAB8_KEY{ "switchToTab8" };
static constexpr std::string_view OPENSETTINGS_KEY{ "openSettings" };

// Specifically use a map here over an unordered_map. We want to be able to
// iterate over these entries in-order when we're serializing the keybindings.
// HERE BE DRAGONS:
// These are string_views that are being used as keys. These string_views are
// just pointers to other strings. This could be dangerous, if the map outlived
// the actual strings being pointed to. However, since both these strings and
// the map are all const for the lifetime of the app, we have nothing to worry
// about here.
static const std::map<std::string_view, ShortcutAction, std::less<>> commandNames {
    { COPYTEXT_KEY, ShortcutAction::CopyText },
    { PASTETEXT_KEY, ShortcutAction::PasteText },
    { NEWTAB_KEY, ShortcutAction::NewTab },
    { NEWTABWITHPROFILE0_KEY, ShortcutAction::NewTabProfile0 },
    { NEWTABWITHPROFILE1_KEY, ShortcutAction::NewTabProfile1 },
    { NEWTABWITHPROFILE2_KEY, ShortcutAction::NewTabProfile2 },
    { NEWTABWITHPROFILE3_KEY, ShortcutAction::NewTabProfile3 },
    { NEWTABWITHPROFILE4_KEY, ShortcutAction::NewTabProfile4 },
    { NEWTABWITHPROFILE5_KEY, ShortcutAction::NewTabProfile5 },
    { NEWTABWITHPROFILE6_KEY, ShortcutAction::NewTabProfile6 },
    { NEWTABWITHPROFILE7_KEY, ShortcutAction::NewTabProfile7 },
    { NEWTABWITHPROFILE8_KEY, ShortcutAction::NewTabProfile8 },
    { NEWWINDOW_KEY, ShortcutAction::NewWindow },
    { CLOSEWINDOW_KEY, ShortcutAction::CloseWindow },
    { CLOSETAB_KEY, ShortcutAction::CloseTab },
    { NEXTTAB_KEY, ShortcutAction::NextTab },
    { PREVTAB_KEY, ShortcutAction::PrevTab },
    { INCREASEFONTSIZE_KEY, ShortcutAction::IncreaseFontSize },
    { DECREASEFONTSIZE_KEY, ShortcutAction::DecreaseFontSize },
    { SCROLLUP_KEY, ShortcutAction::ScrollUp },
    { SCROLLDOWN_KEY, ShortcutAction::ScrollDown },
    { SCROLLUPPAGE_KEY, ShortcutAction::ScrollUpPage },
    { SCROLLDOWNPAGE_KEY, ShortcutAction::ScrollDownPage },
    { SWITCHTOTAB0_KEY, ShortcutAction::SwitchToTab0 },
    { SWITCHTOTAB1_KEY, ShortcutAction::SwitchToTab1 },
    { SWITCHTOTAB2_KEY, ShortcutAction::SwitchToTab2 },
    { SWITCHTOTAB3_KEY, ShortcutAction::SwitchToTab3 },
    { SWITCHTOTAB4_KEY, ShortcutAction::SwitchToTab4 },
    { SWITCHTOTAB5_KEY, ShortcutAction::SwitchToTab5 },
    { SWITCHTOTAB6_KEY, ShortcutAction::SwitchToTab6 },
    { SWITCHTOTAB7_KEY, ShortcutAction::SwitchToTab7 },
    { SWITCHTOTAB8_KEY, ShortcutAction::SwitchToTab8 },
};

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

    jsonObject[JsonKey(KEYS_KEY)] = keysArray;
    jsonObject[JsonKey(COMMAND_KEY)] = actionName.data();

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
            const auto commandString = value[JsonKey(COMMAND_KEY)];
            const auto keys = value[JsonKey(KEYS_KEY)];

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
