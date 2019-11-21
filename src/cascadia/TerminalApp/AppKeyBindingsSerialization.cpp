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
static constexpr std::string_view CopyTextWithoutNewlinesKey{ "copyTextWithoutNewlines" }; // Legacy
static constexpr std::string_view PasteTextKey{ "paste" };
static constexpr std::string_view OpenNewTabDropdownKey{ "openNewTabDropdown" };
static constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
static constexpr std::string_view NewTabKey{ "newTab" };
static constexpr std::string_view NewTabWithProfile0Key{ "newTabProfile0" }; // Legacy
static constexpr std::string_view NewTabWithProfile1Key{ "newTabProfile1" }; // Legacy
static constexpr std::string_view NewTabWithProfile2Key{ "newTabProfile2" }; // Legacy
static constexpr std::string_view NewTabWithProfile3Key{ "newTabProfile3" }; // Legacy
static constexpr std::string_view NewTabWithProfile4Key{ "newTabProfile4" }; // Legacy
static constexpr std::string_view NewTabWithProfile5Key{ "newTabProfile5" }; // Legacy
static constexpr std::string_view NewTabWithProfile6Key{ "newTabProfile6" }; // Legacy
static constexpr std::string_view NewTabWithProfile7Key{ "newTabProfile7" }; // Legacy
static constexpr std::string_view NewTabWithProfile8Key{ "newTabProfile8" }; // Legacy
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
static constexpr std::string_view SwitchToTab0Key{ "switchToTab0" }; // Legacy
static constexpr std::string_view SwitchToTab1Key{ "switchToTab1" }; // Legacy
static constexpr std::string_view SwitchToTab2Key{ "switchToTab2" }; // Legacy
static constexpr std::string_view SwitchToTab3Key{ "switchToTab3" }; // Legacy
static constexpr std::string_view SwitchToTab4Key{ "switchToTab4" }; // Legacy
static constexpr std::string_view SwitchToTab5Key{ "switchToTab5" }; // Legacy
static constexpr std::string_view SwitchToTab6Key{ "switchToTab6" }; // Legacy
static constexpr std::string_view SwitchToTab7Key{ "switchToTab7" }; // Legacy
static constexpr std::string_view SwitchToTab8Key{ "switchToTab8" }; // Legacy
static constexpr std::string_view OpenSettingsKey{ "openSettings" }; // Legacy
static constexpr std::string_view SplitHorizontalKey{ "splitHorizontal" };
static constexpr std::string_view SplitVerticalKey{ "splitVertical" };
static constexpr std::string_view ResizePaneKey{ "resizePane" };
static constexpr std::string_view ResizePaneLeftKey{ "resizePaneLeft" }; // Legacy
static constexpr std::string_view ResizePaneRightKey{ "resizePaneRight" }; // Legacy
static constexpr std::string_view ResizePaneUpKey{ "resizePaneUp" }; // Legacy
static constexpr std::string_view ResizePaneDownKey{ "resizePaneDown" }; // Legacy
static constexpr std::string_view MoveFocusKey{ "moveFocus" };
static constexpr std::string_view MoveFocusLeftKey{ "moveFocusLeft" }; // Legacy
static constexpr std::string_view MoveFocusRightKey{ "moveFocusRight" }; // Legacy
static constexpr std::string_view MoveFocusUpKey{ "moveFocusUp" }; // Legacy
static constexpr std::string_view MoveFocusDownKey{ "moveFocusDown" }; // Legacy
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
    { CopyTextWithoutNewlinesKey, ShortcutAction::CopyTextWithoutNewlines },
    { PasteTextKey, ShortcutAction::PasteText },
    { OpenNewTabDropdownKey, ShortcutAction::OpenNewTabDropdown },
    { DuplicateTabKey, ShortcutAction::DuplicateTab },
    { NewTabKey, ShortcutAction::NewTab },
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
    { ResizePaneKey, ShortcutAction::ResizePane },
    { ResizePaneLeftKey, ShortcutAction::ResizePaneLeft },
    { ResizePaneRightKey, ShortcutAction::ResizePaneRight },
    { ResizePaneUpKey, ShortcutAction::ResizePaneUp },
    { ResizePaneDownKey, ShortcutAction::ResizePaneDown },
    { MoveFocusKey, ShortcutAction::MoveFocus },
    { MoveFocusLeftKey, ShortcutAction::MoveFocusLeft },
    { MoveFocusRightKey, ShortcutAction::MoveFocusRight },
    { MoveFocusUpKey, ShortcutAction::MoveFocusUp },
    { MoveFocusDownKey, ShortcutAction::MoveFocusDown },
    { OpenSettingsKey, ShortcutAction::OpenSettings },
    { ToggleFullscreenKey, ShortcutAction::ToggleFullscreen },
    { UnboundKey, ShortcutAction::Invalid },
};

// Function Description:
// - Creates a function that can be used to generate a MoveFocusArgs for the
//   legacy MoveFocus[Direction] actions. These actions don't accept args from
//   json, instead, they just return a MoveFocusArgs with the Direction already
//   per-defined, based on the input param.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - direction: the direction to create the parse function for.
// Return Value:
// - A function that can be used to "parse" json into one of the legacy
//   MoveFocus[Direction] args.
std::function<IActionArgs(const Json::Value&)> LegacyParseMoveFocusArgs(Direction direction)
{
    auto pfn = [direction](const Json::Value & /*value*/) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::MoveFocusArgs>();
        args->Direction(direction);
        return *args;
    };
    return pfn;
}

// Function Description:
// - Creates a function that can be used to generate a ResizePaneArgs for the
//   legacy ResizePane[Direction] actions. These actions don't accept args from
//   json, instead, they just return a ResizePaneArgs with the Direction already
//   per-defined, based on the input param.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - direction: the direction to create the parse function for.
// Return Value:
// - A function that can be used to "parse" json into one of the legacy
//   ResizePane[Direction] args.
std::function<IActionArgs(const Json::Value&)> LegacyParseResizePaneArgs(Direction direction)
{
    auto pfn = [direction](const Json::Value & /*value*/) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::ResizePaneArgs>();
        args->Direction(direction);
        return *args;
    };
    return pfn;
}

// Function Description:
// - Creates a function that can be used to generate a NewTabWithProfileArgs for
//   the legacy NewTabWithProfile[Index] actions. These actions don't accept
//   args from json, instead, they just return a NewTabWithProfileArgs with the
//   index already per-defined, based on the input param.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - index: the profile index to create the parse function for.
// Return Value:
// - A function that can be used to "parse" json into one of the legacy
//   NewTabWithProfile[Index] args.
std::function<IActionArgs(const Json::Value&)> LegacyParseNewTabWithProfileArgs(int index)
{
    auto pfn = [index](const Json::Value & /*value*/) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::NewTabArgs>();
        args->ProfileIndex(index);
        return *args;
    };
    return pfn;
}

// Function Description:
// - Creates a function that can be used to generate a SwitchToTabArgs for the
//   legacy SwitchToTab[Index] actions. These actions don't accept args from
//   json, instead, they just return a SwitchToTabArgs with the index already
//   per-defined, based on the input param.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - index: the tab index to create the parse function for.
// Return Value:
// - A function that can be used to "parse" json into one of the legacy
//   SwitchToTab[Index] args.
std::function<IActionArgs(const Json::Value&)> LegacyParseSwitchToTabArgs(int index)
{
    auto pfn = [index](const Json::Value & /*value*/) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::SwitchToTabArgs>();
        args->TabIndex(index);
        return *args;
    };
    return pfn;
}

// Function Description:
// - Used to generate a CopyTextArgs for the legacy CopyTextWithoutNewlines
//   action.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - direction: the direction to create the parse function for.
// Return Value:
// - A CopyTextArgs with TrimWhitespace set to true, to emulate "CopyTextWithoutNewlines".
IActionArgs LegacyParseCopyTextWithoutNewlinesArgs(const Json::Value& /*json*/)
{
    auto args = winrt::make_self<winrt::TerminalApp::implementation::CopyTextArgs>();
    args->TrimWhitespace(true);
    return *args;
};

// Function Description:
// - Used to generate a AdjustFontSizeArgs for IncreaseFontSize/DecreaseFontSize
//   actions with a delta of 1/-1.
// - TODO: GH#1069 Remove this before 1.0, and force an upgrade to the new args.
// Arguments:
// - delta: the font size delta to create the parse function for.
// Return Value:
// - A function that can be used to "parse" json into an AdjustFontSizeArgs.
std::function<IActionArgs(const Json::Value&)> LegacyParseAdjustFontSizeArgs(int delta)
{
    auto pfn = [delta](const Json::Value & /*value*/) -> IActionArgs {
        auto args = winrt::make_self<winrt::TerminalApp::implementation::AdjustFontSizeArgs>();
        args->Delta(delta);
        return *args;
    };
    return pfn;
}

// This is a map of ShortcutAction->function<IActionArgs(Json::Value)>. It holds
// a set of deserializer functions that can be used to deserialize a IActionArgs
// from json. Each type of IActionArgs that can accept arbitrary args should be
// placed into this map, with the corresponding deserializer function as the
// value.
static const std::map<ShortcutAction, std::function<IActionArgs(const Json::Value&)>, std::less<>> argParsers{
    { ShortcutAction::CopyText, winrt::TerminalApp::implementation::CopyTextArgs::FromJson },
    { ShortcutAction::CopyTextWithoutNewlines, LegacyParseCopyTextWithoutNewlinesArgs },

    { ShortcutAction::NewTab, winrt::TerminalApp::implementation::NewTabArgs::FromJson },
    { ShortcutAction::NewTabProfile0, LegacyParseNewTabWithProfileArgs(0) },
    { ShortcutAction::NewTabProfile1, LegacyParseNewTabWithProfileArgs(1) },
    { ShortcutAction::NewTabProfile2, LegacyParseNewTabWithProfileArgs(2) },
    { ShortcutAction::NewTabProfile3, LegacyParseNewTabWithProfileArgs(3) },
    { ShortcutAction::NewTabProfile4, LegacyParseNewTabWithProfileArgs(4) },
    { ShortcutAction::NewTabProfile5, LegacyParseNewTabWithProfileArgs(5) },
    { ShortcutAction::NewTabProfile6, LegacyParseNewTabWithProfileArgs(6) },
    { ShortcutAction::NewTabProfile7, LegacyParseNewTabWithProfileArgs(7) },
    { ShortcutAction::NewTabProfile8, LegacyParseNewTabWithProfileArgs(8) },

    { ShortcutAction::SwitchToTab, winrt::TerminalApp::implementation::SwitchToTabArgs::FromJson },
    { ShortcutAction::SwitchToTab0, LegacyParseSwitchToTabArgs(0) },
    { ShortcutAction::SwitchToTab1, LegacyParseSwitchToTabArgs(1) },
    { ShortcutAction::SwitchToTab2, LegacyParseSwitchToTabArgs(2) },
    { ShortcutAction::SwitchToTab3, LegacyParseSwitchToTabArgs(3) },
    { ShortcutAction::SwitchToTab4, LegacyParseSwitchToTabArgs(4) },
    { ShortcutAction::SwitchToTab5, LegacyParseSwitchToTabArgs(5) },
    { ShortcutAction::SwitchToTab6, LegacyParseSwitchToTabArgs(6) },
    { ShortcutAction::SwitchToTab7, LegacyParseSwitchToTabArgs(7) },
    { ShortcutAction::SwitchToTab8, LegacyParseSwitchToTabArgs(8) },

    { ShortcutAction::ResizePane, winrt::TerminalApp::implementation::ResizePaneArgs::FromJson },
    { ShortcutAction::ResizePaneLeft, LegacyParseResizePaneArgs(Direction::Left) },
    { ShortcutAction::ResizePaneRight, LegacyParseResizePaneArgs(Direction::Right) },
    { ShortcutAction::ResizePaneUp, LegacyParseResizePaneArgs(Direction::Up) },
    { ShortcutAction::ResizePaneDown, LegacyParseResizePaneArgs(Direction::Down) },

    { ShortcutAction::MoveFocus, winrt::TerminalApp::implementation::MoveFocusArgs::FromJson },
    { ShortcutAction::MoveFocusLeft, LegacyParseMoveFocusArgs(Direction::Left) },
    { ShortcutAction::MoveFocusRight, LegacyParseMoveFocusArgs(Direction::Right) },
    { ShortcutAction::MoveFocusUp, LegacyParseMoveFocusArgs(Direction::Up) },
    { ShortcutAction::MoveFocusDown, LegacyParseMoveFocusArgs(Direction::Down) },

    { ShortcutAction::DecreaseFontSize, LegacyParseAdjustFontSizeArgs(-1) },
    { ShortcutAction::IncreaseFontSize, LegacyParseAdjustFontSizeArgs(1) },

    { ShortcutAction::Invalid, nullptr },
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

        if (keys)
        {
            if (!keys.isArray() || keys.size() != 1)
            {
                continue;
            }
            const auto keyChordString = winrt::to_hstring(keys[0].asString());
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
            //    the "command" object to the arg parser, for furhter parsing.

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
            const auto deserializersIter = argParsers.find(action);
            if (deserializersIter != argParsers.end())
            {
                auto pfn = deserializersIter->second;
                if (pfn)
                {
                    args = pfn(argsVal);
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
}
