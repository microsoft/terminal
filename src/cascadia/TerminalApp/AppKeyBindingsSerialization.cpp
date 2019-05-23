// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindingsSerialization.h"
#include "KeyChordSerialization.h"
#include "Utils.h"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view KEYS_KEY_2{ "keys" };
static constexpr std::string_view COMMAND_KEY_2{ "command" };

static constexpr std::wstring_view COPYTEXT_KEY{ L"copy" };
static constexpr std::wstring_view PASTETEXT_KEY{ L"paste" };
static constexpr std::wstring_view NEWTAB_KEY{ L"newTab" };
static constexpr std::wstring_view NEWTABWITHPROFILE0_KEY{ L"newTabProfile0" };
static constexpr std::wstring_view NEWTABWITHPROFILE1_KEY{ L"newTabProfile1" };
static constexpr std::wstring_view NEWTABWITHPROFILE2_KEY{ L"newTabProfile2" };
static constexpr std::wstring_view NEWTABWITHPROFILE3_KEY{ L"newTabProfile3" };
static constexpr std::wstring_view NEWTABWITHPROFILE4_KEY{ L"newTabProfile4" };
static constexpr std::wstring_view NEWTABWITHPROFILE5_KEY{ L"newTabProfile5" };
static constexpr std::wstring_view NEWTABWITHPROFILE6_KEY{ L"newTabProfile6" };
static constexpr std::wstring_view NEWTABWITHPROFILE7_KEY{ L"newTabProfile7" };
static constexpr std::wstring_view NEWTABWITHPROFILE8_KEY{ L"newTabProfile8" };
static constexpr std::wstring_view NEWWINDOW_KEY{ L"newWindow" };
static constexpr std::wstring_view CLOSEWINDOW_KEY{ L"closeWindow" };
static constexpr std::wstring_view CLOSETAB_KEY{ L"closeTab" };
static constexpr std::wstring_view SWITCHTOTAB_KEY{ L"switchToTab" };
static constexpr std::wstring_view NEXTTAB_KEY{ L"nextTab" };
static constexpr std::wstring_view PREVTAB_KEY{ L"prevTab" };
static constexpr std::wstring_view INCREASEFONTSIZE_KEY{ L"increaseFontSize" };
static constexpr std::wstring_view DECREASEFONTSIZE_KEY{ L"decreaseFontSize" };
static constexpr std::wstring_view SCROLLUP_KEY{ L"scrollUp" };
static constexpr std::wstring_view SCROLLDOWN_KEY{ L"scrollDown" };
static constexpr std::wstring_view SCROLLUPPAGE_KEY{ L"scrollUpPage" };
static constexpr std::wstring_view SCROLLDOWNPAGE_KEY{ L"scrollDownPage" };
static constexpr std::wstring_view SWITCHTOTAB0_KEY{ L"switchToTab0" };
static constexpr std::wstring_view SWITCHTOTAB1_KEY{ L"switchToTab1" };
static constexpr std::wstring_view SWITCHTOTAB2_KEY{ L"switchToTab2" };
static constexpr std::wstring_view SWITCHTOTAB3_KEY{ L"switchToTab3" };
static constexpr std::wstring_view SWITCHTOTAB4_KEY{ L"switchToTab4" };
static constexpr std::wstring_view SWITCHTOTAB5_KEY{ L"switchToTab5" };
static constexpr std::wstring_view SWITCHTOTAB6_KEY{ L"switchToTab6" };
static constexpr std::wstring_view SWITCHTOTAB7_KEY{ L"switchToTab7" };
static constexpr std::wstring_view SWITCHTOTAB8_KEY{ L"switchToTab8" };
static constexpr std::wstring_view OPENSETTINGS_KEY{ L"openSettings" };

// Specifically use a map here over an unordered_map. We want to be able to
// iterate over these entries in-order when we're serializing the keybindings.
static const std::map<std::wstring_view, ShortcutAction> commandNames {
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
// - Small helper to insert a given KeyChord, ShortcutAction pair into the
//   given json array
// Arguments:
// - bindingsArray: The JsonArray to insert the object into.
// - chord: The KeyChord to serailize and place in the json array
// - actionName: the name of the ShortcutAction to use with this KeyChord
static void _AddShortcutToJsonArray2(Json::Value& bindingsArray,
                                     const KeyChord& chord,
                                     const std::wstring_view& actionName)
{
    const auto keyString = KeyChordSerialization::ToString(chord);
    if (keyString == L"")
    {
        return;
    }

    Json::Value jsonObject;
    Json::Value keysArray;
    keysArray.append(winrt::to_string(keyString));

    jsonObject[KEYS_KEY_2.data()] = keysArray;
    jsonObject[COMMAND_KEY_2.data()] = winrt::to_string(actionName);

    bindingsArray.append(jsonObject);
}

Json::Value AppKeyBindingsSerialization::ToJson(const winrt::TerminalApp::AppKeyBindings& bindings)
{
    Json::Value bindingsArray;

    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : commandNames)
    {
        const auto searchedForName = actionName.first;
        const auto searchedForAction = actionName.second;
        const auto chord = bindings.LookupKeyBinding(searchedForAction);
        if (chord)
        {
            _AddShortcutToJsonArray2(bindingsArray, chord, searchedForName);
        }
    }

    return bindingsArray;
}

winrt::TerminalApp::AppKeyBindings AppKeyBindingsSerialization::FromJson(const Json::Value& json)
{
    winrt::TerminalApp::AppKeyBindings newBindings{};

    for (const auto& value : json)
    {
        if (value.isObject())
        {
            const auto commandString = value[COMMAND_KEY_2.data()];
            const auto keys = value[KEYS_KEY_2.data()];

            if (commandString && keys)
            {
                if (!keys.isArray() || keys.size() != 1)
                {
                    continue;
                }
                const auto keyChordString = winrt::to_hstring(keys[0].asString());
                ShortcutAction action;

                // Try matching the command to one we have
                auto found = commandNames.find(GetWstringFromJson(commandString));
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
                    auto chord = KeyChordSerialization::FromString(keyChordString);
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
