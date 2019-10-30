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
static constexpr std::string_view ArgsKey{ "args" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

static constexpr std::string_view CopyTextKey{ "copy" };
static constexpr std::string_view CopyTextWithoutNewlinesKey{ "copyTextWithoutNewlines" };
static constexpr std::string_view PasteTextKey{ "paste" };
static constexpr std::string_view NewTabKey{ "newTab" };
static constexpr std::string_view OpenNewTabDropdownKey{ "openNewTabDropdown" };
static constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
static constexpr std::string_view NewTabWithProfile0Key{ "newTabProfile0" };
static constexpr std::string_view NewTabWithProfile1Key{ "newTabProfile1" };
static constexpr std::string_view NewTabWithProfile2Key{ "newTabProfile2" };
static constexpr std::string_view NewTabWithProfile3Key{ "newTabProfile3" };
static constexpr std::string_view NewTabWithProfile4Key{ "newTabProfile4" };
static constexpr std::string_view NewTabWithProfile5Key{ "newTabProfile5" };
static constexpr std::string_view NewTabWithProfile6Key{ "newTabProfile6" };
static constexpr std::string_view NewTabWithProfile7Key{ "newTabProfile7" };
static constexpr std::string_view NewTabWithProfile8Key{ "newTabProfile8" };
static constexpr std::string_view NewWindowKey{ "newWindow" };
static constexpr std::string_view CloseWindowKey{ "closeWindow" };
static constexpr std::string_view CloseTabKey{ "closeTab" };
static constexpr std::string_view ClosePaneKey{ "closePane" };
static constexpr std::string_view SwitchtoTabKey{ "switchToTab" };
static constexpr std::string_view NextTabKey{ "nextTab" };
static constexpr std::string_view PrevTabKey{ "prevTab" };
static constexpr std::string_view IncreaseFontSizeKey{ "increaseFontSize" };
static constexpr std::string_view DecreaseFontSizeKey{ "decreaseFontSize" };
static constexpr std::string_view ScrollupKey{ "scrollUp" };
static constexpr std::string_view ScrolldownKey{ "scrollDown" };
static constexpr std::string_view ScrolluppageKey{ "scrollUpPage" };
static constexpr std::string_view ScrolldownpageKey{ "scrollDownPage" };
static constexpr std::string_view SwitchToTabKey{ "switchToTab" };
static constexpr std::string_view SwitchToTab0Key{ "switchToTab0" };
static constexpr std::string_view SwitchToTab1Key{ "switchToTab1" };
static constexpr std::string_view SwitchToTab2Key{ "switchToTab2" };
static constexpr std::string_view SwitchToTab3Key{ "switchToTab3" };
static constexpr std::string_view SwitchToTab4Key{ "switchToTab4" };
static constexpr std::string_view SwitchToTab5Key{ "switchToTab5" };
static constexpr std::string_view SwitchToTab6Key{ "switchToTab6" };
static constexpr std::string_view SwitchToTab7Key{ "switchToTab7" };
static constexpr std::string_view SwitchToTab8Key{ "switchToTab8" };
static constexpr std::string_view OpenSettingsKey{ "openSettings" };
static constexpr std::string_view SplitHorizontalKey{ "splitHorizontal" };
static constexpr std::string_view SplitVerticalKey{ "splitVertical" };
static constexpr std::string_view ResizePaneLeftKey{ "resizePaneLeft" };
static constexpr std::string_view ResizePaneRightKey{ "resizePaneRight" };
static constexpr std::string_view ResizePaneUpKey{ "resizePaneUp" };
static constexpr std::string_view ResizePaneDownKey{ "resizePaneDown" };
static constexpr std::string_view MoveFocusLeftKey{ "moveFocusLeft" };
static constexpr std::string_view MoveFocusRightKey{ "moveFocusRight" };
static constexpr std::string_view MoveFocusUpKey{ "moveFocusUp" };
static constexpr std::string_view MoveFocusDownKey{ "moveFocusDown" };

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
    { CopyTextWithoutNewlinesKey, ShortcutAction::CopyTextWithoutNewlines },
    { PasteTextKey, ShortcutAction::PasteText },
    { NewTabKey, ShortcutAction::NewTab },
    { OpenNewTabDropdownKey, ShortcutAction::OpenNewTabDropdown },
    { DuplicateTabKey, ShortcutAction::DuplicateTab },
    { NewTabWithProfile0Key, ShortcutAction::NewTabProfile0 },
    { NewTabWithProfile1Key, ShortcutAction::NewTabProfile1 },
    { NewTabWithProfile2Key, ShortcutAction::NewTabProfile2 },
    { NewTabWithProfile3Key, ShortcutAction::NewTabProfile3 },
    { NewTabWithProfile4Key, ShortcutAction::NewTabProfile4 },
    { NewTabWithProfile5Key, ShortcutAction::NewTabProfile5 },
    { NewTabWithProfile6Key, ShortcutAction::NewTabProfile6 },
    { NewTabWithProfile7Key, ShortcutAction::NewTabProfile7 },
    { NewTabWithProfile8Key, ShortcutAction::NewTabProfile8 },
    { NewWindowKey, ShortcutAction::NewWindow },
    { CloseWindowKey, ShortcutAction::CloseWindow },
    { CloseTabKey, ShortcutAction::CloseTab },
    { ClosePaneKey, ShortcutAction::ClosePane },
    { NextTabKey, ShortcutAction::NextTab },
    { PrevTabKey, ShortcutAction::PrevTab },
    { IncreaseFontSizeKey, ShortcutAction::IncreaseFontSize },
    { DecreaseFontSizeKey, ShortcutAction::DecreaseFontSize },
    { ScrollupKey, ShortcutAction::ScrollUp },
    { ScrolldownKey, ShortcutAction::ScrollDown },
    { ScrolluppageKey, ShortcutAction::ScrollUpPage },
    { ScrolldownpageKey, ShortcutAction::ScrollDownPage },
    { SwitchToTabKey, ShortcutAction::SwitchToTab },
    { SwitchToTab0Key, ShortcutAction::SwitchToTab0 },
    { SwitchToTab1Key, ShortcutAction::SwitchToTab1 },
    { SwitchToTab2Key, ShortcutAction::SwitchToTab2 },
    { SwitchToTab3Key, ShortcutAction::SwitchToTab3 },
    { SwitchToTab4Key, ShortcutAction::SwitchToTab4 },
    { SwitchToTab5Key, ShortcutAction::SwitchToTab5 },
    { SwitchToTab6Key, ShortcutAction::SwitchToTab6 },
    { SwitchToTab7Key, ShortcutAction::SwitchToTab7 },
    { SwitchToTab8Key, ShortcutAction::SwitchToTab8 },
    { SplitHorizontalKey, ShortcutAction::SplitHorizontal },
    { SplitVerticalKey, ShortcutAction::SplitVertical },
    { ResizePaneLeftKey, ShortcutAction::ResizePaneLeft },
    { ResizePaneRightKey, ShortcutAction::ResizePaneRight },
    { ResizePaneUpKey, ShortcutAction::ResizePaneUp },
    { ResizePaneDownKey, ShortcutAction::ResizePaneDown },
    { MoveFocusLeftKey, ShortcutAction::MoveFocusLeft },
    { MoveFocusRightKey, ShortcutAction::MoveFocusRight },
    { MoveFocusUpKey, ShortcutAction::MoveFocusUp },
    { MoveFocusDownKey, ShortcutAction::MoveFocusDown },
    { OpenSettingsKey, ShortcutAction::OpenSettings },
    { UnboundKey, ShortcutAction::Invalid },
};

std::function<IActionArgs(const Json::Value&)> LegacyParseSwitchToTabArgs(int index)
{
    auto pfn = [index](const Json::Value& value) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::SwitchToTabArgs>();
        args->TabIndex(index);
        return *args;
    };
    return pfn;
}

IActionArgs LegacyParseCopyTextWithoutNewlinesArgs(const Json::Value& json)
{
    auto args = winrt::make_self<winrt::TerminalApp::implementation::CopyTextArgs>();
    args->TrimWhitespace(true);
    return *args;
};

static const std::map<ShortcutAction,
                      std::function<IActionArgs(const Json::Value&)>,
                      std::less<>>
    argParsers{
        { ShortcutAction::CopyText, winrt::TerminalApp::implementation::CopyTextArgs::FromJson },
        { ShortcutAction::CopyTextWithoutNewlines, LegacyParseCopyTextWithoutNewlinesArgs },
        { ShortcutAction::SwitchToTab, winrt::TerminalApp::implementation::SwitchToTabArgs::FromJson2 },
        { ShortcutAction::SwitchToTab0, LegacyParseSwitchToTabArgs(0) },
        { ShortcutAction::SwitchToTab1, LegacyParseSwitchToTabArgs(1) },
        { ShortcutAction::SwitchToTab2, LegacyParseSwitchToTabArgs(2) },
        { ShortcutAction::SwitchToTab3, LegacyParseSwitchToTabArgs(3) },
        { ShortcutAction::SwitchToTab4, LegacyParseSwitchToTabArgs(4) },
        { ShortcutAction::SwitchToTab5, LegacyParseSwitchToTabArgs(5) },
        { ShortcutAction::SwitchToTab6, LegacyParseSwitchToTabArgs(6) },
        { ShortcutAction::SwitchToTab7, LegacyParseSwitchToTabArgs(7) },
        { ShortcutAction::SwitchToTab8, LegacyParseSwitchToTabArgs(8) },
        { ShortcutAction::Invalid, nullptr },
    };

// static const std::map<ShortcutAction,
//                       std::function<IActionArgs(const Json::Value&)>,
//                       std::less<>>
//     argParsers2{
//         // { ShortcutAction::CopyText, winrt::TerminalApp::implementation::CopyTextArgs::FromJson },
//         // { ShortcutAction::CopyTextWithoutNewlines, LegacyParseCopyTextWithoutNewlinesArgs },
//         { ShortcutAction::SwitchToTab, winrt::TerminalApp::implementation::SwitchToTabArgs::FromJson2 },
//         // { ShortcutAction::SwitchToTab0, LegacyParseSwitchToTabArgs(0) },
//         // { ShortcutAction::SwitchToTab1, LegacyParseSwitchToTabArgs(1) },
//         // { ShortcutAction::SwitchToTab2, LegacyParseSwitchToTabArgs(2) },
//         // { ShortcutAction::SwitchToTab3, LegacyParseSwitchToTabArgs(3) },
//         // { ShortcutAction::SwitchToTab4, LegacyParseSwitchToTabArgs(4) },
//         // { ShortcutAction::SwitchToTab5, LegacyParseSwitchToTabArgs(5) },
//         // { ShortcutAction::SwitchToTab6, LegacyParseSwitchToTabArgs(6) },
//         // { ShortcutAction::SwitchToTab7, LegacyParseSwitchToTabArgs(7) },
//         // { ShortcutAction::SwitchToTab8, LegacyParseSwitchToTabArgs(8) },
//         { ShortcutAction::Invalid, nullptr },
//     };

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
Json::Value winrt::TerminalApp::implementation::AppKeyBindings::ToJson()
{
    Json::Value bindingsArray;

    // Iterate over all the possible actions in the names list, and see if
    // it has a binding.
    for (auto& actionName : commandNames)
    {
        const auto searchedForName = actionName.first;
        const auto searchedForAction = actionName.second;

        if (const auto chord{ GetKeyBinding(searchedForAction) })
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
// - Applies the deserialized keybindings to the provided `bindings` object. If
//   a key chord in `json` is already bound to an action, that chord will be
//   overwritten with the new action. If a chord is bound to `null` or
//   `"unbound"`, then we'll clear the keybinding from the existing keybindings.
// Arguments:
// - json: and array of JsonObject's to deserialize into our _keyShortcuts mapping.
void winrt::TerminalApp::implementation::AppKeyBindings::LayerJson(const Json::Value& json)
{
    for (const auto& value : json)
    {
        if (!value.isObject())
        {
            continue;
        }

        const auto commandVal = value[JsonKey(CommandKey)];
        const auto keys = value[JsonKey(KeysKey)];
        const auto argsVal = value[JsonKey(ArgsKey)];

        if (keys)
        {
            if (!keys.isArray() || keys.size() != 1)
            {
                continue;
            }
            const auto keyChordString = winrt::to_hstring(keys[0].asString());
            // Invalid is our placeholder that the action was not parsed.
            ShortcutAction action = ShortcutAction::Invalid;

            // Only try to parse the action if it's actually a string value.
            // `null` will not pass this check.
            if (commandVal.isString())
            {
                auto commandString = commandVal.asString();

                // Try matching the command to one we have. If we can't find the
                // action name in our list of names, let's just unbind that key.
                const auto found = commandNames.find(commandString);
                if (found != commandNames.end())
                {
                    action = found->second;
                }
            }

            IActionArgs args{ nullptr };
            if (argsVal)
            {
                // The binding included args. Lets create try parsing them.
                const auto deserializersIter = argParsers.find(action);
                if (deserializersIter != argParsers.end())
                {
                    auto pfn = deserializersIter->second;
                    if (pfn)
                    {
                        args = pfn(argsVal);
                    }
                }
            }

            // IActionArgs args{ nullptr };
            // if (argsVal)
            // {
            //     auto testArgs = winrt::make_self<SwitchToTabArgs>();
            //     testArgs->TabIndex(5);
            //     // auto foo = *testArgs;
            //     args = *testArgs;
            // }

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
}
