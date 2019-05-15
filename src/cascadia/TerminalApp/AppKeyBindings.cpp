<<<<<<< HEAD
﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

static const std::wstring KEYS_KEY{ L"keys" };
static const std::wstring COMMAND_KEY{ L"command" };

static const std::wstring COPYTEXT_KEY{ L"copy" };
static const std::wstring PASTETEXT_KEY{ L"paste" };
static const std::wstring NEWTAB_KEY{ L"newTab" };
static const std::wstring NEWTABWITHPROFILE0_KEY{ L"newTabProfile0" };
static const std::wstring NEWTABWITHPROFILE1_KEY{ L"newTabProfile1" };
static const std::wstring NEWTABWITHPROFILE2_KEY{ L"newTabProfile2" };
static const std::wstring NEWTABWITHPROFILE3_KEY{ L"newTabProfile3" };
static const std::wstring NEWTABWITHPROFILE4_KEY{ L"newTabProfile4" };
static const std::wstring NEWTABWITHPROFILE5_KEY{ L"newTabProfile5" };
static const std::wstring NEWTABWITHPROFILE6_KEY{ L"newTabProfile6" };
static const std::wstring NEWTABWITHPROFILE7_KEY{ L"newTabProfile7" };
static const std::wstring NEWTABWITHPROFILE8_KEY{ L"newTabProfile8" };
static const std::wstring NEWTABWITHPROFILE9_KEY{ L"newTabProfile9" };
static const std::wstring NEWWINDOW_KEY{ L"newWindow" };
static const std::wstring CLOSEWINDOW_KEY{ L"closeWindow" };
static const std::wstring CLOSETAB_KEY{ L"closeTab" };
static const std::wstring SWITCHTOTAB_KEY{ L"switchToTab" };
static const std::wstring NEXTTAB_KEY{ L"nextTab" };
static const std::wstring PREVTAB_KEY{ L"prevTab" };
static const std::wstring INCREASEFONTSIZE_KEY{ L"increaseFontSize" };
static const std::wstring DECREASEFONTSIZE_KEY{ L"decreaseFontSize" };
static const std::wstring SCROLLUP_KEY{ L"scrollUp" };
static const std::wstring SCROLLDOWN_KEY{ L"scrollDown" };
static const std::wstring SCROLLUPPAGE_KEY{ L"scrollUpPage" };
static const std::wstring SCROLLDOWNPAGE_KEY{ L"scrollDownPage" };
static const std::wstring SWITCHTOTAB0_KEY{ L"switchToTab0" };
static const std::wstring SWITCHTOTAB1_KEY{ L"switchToTab1" };
static const std::wstring SWITCHTOTAB2_KEY{ L"switchToTab2" };
static const std::wstring SWITCHTOTAB3_KEY{ L"switchToTab3" };
static const std::wstring SWITCHTOTAB4_KEY{ L"switchToTab4" };
static const std::wstring SWITCHTOTAB5_KEY{ L"switchToTab5" };
static const std::wstring SWITCHTOTAB6_KEY{ L"switchToTab6" };
static const std::wstring SWITCHTOTAB7_KEY{ L"switchToTab7" };
static const std::wstring SWITCHTOTAB8_KEY{ L"switchToTab8" };
static const std::wstring SWITCHTOTAB9_KEY{ L"switchToTab9" };
static const std::wstring OPENSETTINGS_KEY{ L"openSettings" };

static const std::vector<std::pair<ShortcutAction, std::wstring>> commandNames {
    { ShortcutAction::CopyText, COPYTEXT_KEY },
    { ShortcutAction::PasteText, PASTETEXT_KEY },
    { ShortcutAction::NewTab, NEWTAB_KEY },
    { ShortcutAction::NewTabProfile0, NEWTABWITHPROFILE0_KEY },
    { ShortcutAction::NewTabProfile1, NEWTABWITHPROFILE1_KEY },
    { ShortcutAction::NewTabProfile2, NEWTABWITHPROFILE2_KEY },
    { ShortcutAction::NewTabProfile3, NEWTABWITHPROFILE3_KEY },
    { ShortcutAction::NewTabProfile4, NEWTABWITHPROFILE4_KEY },
    { ShortcutAction::NewTabProfile5, NEWTABWITHPROFILE5_KEY },
    { ShortcutAction::NewTabProfile6, NEWTABWITHPROFILE6_KEY },
    { ShortcutAction::NewTabProfile7, NEWTABWITHPROFILE7_KEY },
    { ShortcutAction::NewTabProfile8, NEWTABWITHPROFILE8_KEY },
    { ShortcutAction::NewTabProfile9, NEWTABWITHPROFILE9_KEY },
    { ShortcutAction::NewWindow, NEWWINDOW_KEY },
    { ShortcutAction::CloseWindow, CLOSEWINDOW_KEY },
    { ShortcutAction::CloseTab, CLOSETAB_KEY },
    { ShortcutAction::NextTab, NEXTTAB_KEY },
    { ShortcutAction::PrevTab, PREVTAB_KEY },
    { ShortcutAction::IncreaseFontSize, INCREASEFONTSIZE_KEY },
    { ShortcutAction::DecreaseFontSize, DECREASEFONTSIZE_KEY },
    { ShortcutAction::ScrollUp, SCROLLUP_KEY },
    { ShortcutAction::ScrollDown, SCROLLDOWN_KEY },
    { ShortcutAction::ScrollUpPage, SCROLLUPPAGE_KEY },
    { ShortcutAction::ScrollDownPage, SCROLLDOWNPAGE_KEY },
    { ShortcutAction::SwitchToTab0, SWITCHTOTAB0_KEY },
    { ShortcutAction::SwitchToTab1, SWITCHTOTAB1_KEY },
    { ShortcutAction::SwitchToTab2, SWITCHTOTAB2_KEY },
    { ShortcutAction::SwitchToTab3, SWITCHTOTAB3_KEY },
    { ShortcutAction::SwitchToTab4, SWITCHTOTAB4_KEY },
    { ShortcutAction::SwitchToTab5, SWITCHTOTAB5_KEY },
    { ShortcutAction::SwitchToTab6, SWITCHTOTAB6_KEY },
    { ShortcutAction::SwitchToTab7, SWITCHTOTAB7_KEY },
    { ShortcutAction::SwitchToTab8, SWITCHTOTAB8_KEY },
    { ShortcutAction::SwitchToTab9, SWITCHTOTAB9_KEY },
};

namespace winrt::TerminalApp::implementation
{
    void AppKeyBindings::SetKeyBinding(TerminalApp::ShortcutAction const& action,
                                       Settings::KeyChord const& chord)
    {
        _keyShortcuts[chord] = action;
    }

    bool AppKeyBindings::TryKeyChord(Settings::KeyChord const& kc)
    {
        const auto keyIter = _keyShortcuts.find(kc);
        if (keyIter != _keyShortcuts.end())
        {
            const auto action = keyIter->second;
            return _DoAction(action);
        }
        return false;
    }

    bool AppKeyBindings::_DoAction(ShortcutAction action)
    {
        switch (action)
        {
            case ShortcutAction::CopyText:
                _CopyTextHandlers();
                return true;
            case ShortcutAction::PasteText:
                _PasteTextHandlers();
                return true;
            case ShortcutAction::NewTab:
                _NewTabHandlers();
                return true;
            case ShortcutAction::OpenSettings:
                _OpenSettingsHandlers();
                return true;

            case ShortcutAction::NewTabProfile0:
                _NewTabWithProfileHandlers(0);
                return true;
            case ShortcutAction::NewTabProfile1:
                _NewTabWithProfileHandlers(1);
                return true;
            case ShortcutAction::NewTabProfile2:
                _NewTabWithProfileHandlers(2);
                return true;
            case ShortcutAction::NewTabProfile3:
                _NewTabWithProfileHandlers(3);
                return true;
            case ShortcutAction::NewTabProfile4:
                _NewTabWithProfileHandlers(4);
                return true;
            case ShortcutAction::NewTabProfile5:
                _NewTabWithProfileHandlers(5);
                return true;
            case ShortcutAction::NewTabProfile6:
                _NewTabWithProfileHandlers(6);
                return true;
            case ShortcutAction::NewTabProfile7:
                _NewTabWithProfileHandlers(7);
                return true;
            case ShortcutAction::NewTabProfile8:
                _NewTabWithProfileHandlers(8);
                return true;
            case ShortcutAction::NewTabProfile9:
                _NewTabWithProfileHandlers(9);
                return true;

            case ShortcutAction::NewWindow:
                _NewWindowHandlers();
                return true;
            case ShortcutAction::CloseWindow:
                _CloseWindowHandlers();
                return true;
            case ShortcutAction::CloseTab:
                _CloseTabHandlers();
                return true;

            case ShortcutAction::ScrollUp:
                _ScrollUpHandlers();
                return true;
            case ShortcutAction::ScrollDown:
                _ScrollDownHandlers();
                return true;
            case ShortcutAction::ScrollUpPage:
                _ScrollUpPageHandlers();
                return true;
            case ShortcutAction::ScrollDownPage:
                _ScrollDownPageHandlers();
                return true;

            case ShortcutAction::NextTab:
                _NextTabHandlers();
                return true;
            case ShortcutAction::PrevTab:
                _PrevTabHandlers();
                return true;

            case ShortcutAction::SwitchToTab0:
                _SwitchToTabHandlers(0);
                return true;
            case ShortcutAction::SwitchToTab1:
                _SwitchToTabHandlers(1);
                return true;
            case ShortcutAction::SwitchToTab2:
                _SwitchToTabHandlers(2);
                return true;
            case ShortcutAction::SwitchToTab3:
                _SwitchToTabHandlers(3);
                return true;
            case ShortcutAction::SwitchToTab4:
                _SwitchToTabHandlers(4);
                return true;
            case ShortcutAction::SwitchToTab5:
                _SwitchToTabHandlers(5);
                return true;
            case ShortcutAction::SwitchToTab6:
                _SwitchToTabHandlers(6);
                return true;
            case ShortcutAction::SwitchToTab7:
                _SwitchToTabHandlers(7);
                return true;
            case ShortcutAction::SwitchToTab8:
                _SwitchToTabHandlers(8);
                return true;
            case ShortcutAction::SwitchToTab9:
                _SwitchToTabHandlers(9);
                return true;
        }
        return false;
    }

    // -------------------------------- Events ---------------------------------
    DEFINE_EVENT(AppKeyBindings, CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
    DEFINE_EVENT(AppKeyBindings, PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
    DEFINE_EVENT(AppKeyBindings, NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
    DEFINE_EVENT(AppKeyBindings, CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
    DEFINE_EVENT(AppKeyBindings, CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
    DEFINE_EVENT(AppKeyBindings, IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
    DEFINE_EVENT(AppKeyBindings, DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
    DEFINE_EVENT(AppKeyBindings, ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
    DEFINE_EVENT(AppKeyBindings, OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);

    // Method Description:
    // - Deserialize an AppKeyBindings from the key mappings that are in the
    //   array `json`. The json array should contain an array of objects with
    //   both a `command` string and a `keys` array, where `command` i one of
    //   the names listed in `commandNames`, and `keys` is an array of
    //   keypresses. Currently, the array should contain a single string, which
    //   can be deserialized into a KeyChord.
    // Arguments:
    // - json: and arrayof JsonObject's to deserialize into our _keyShortcuts mapping.
    // Return Value:
    // - the newly constructed AppKeyBindings object.
    TerminalApp::AppKeyBindings AppKeyBindings::FromJson(Windows::Data::Json::JsonArray const& json)
    {
        TerminalApp::AppKeyBindings newBindings{};

        for (const auto& value : json)
        {
            if (value.ValueType() == JsonValueType::Object)
            {
                JsonObject obj = value.GetObjectW();
                if (obj.HasKey(COMMAND_KEY) && obj.HasKey(KEYS_KEY))
                {
                    const auto commandString = obj.GetNamedString(COMMAND_KEY);
                    const auto keys = obj.GetNamedArray(KEYS_KEY);
                    if (keys.Size() != 1)
                    {
                        continue;
                    }
                    const auto keyChordString = keys.GetAt(0).GetString();
                    ShortcutAction action;
                    bool parsedSuccessfully = false;
                    // Try matching the command to one we have
                    for (const auto& pair : commandNames)
                    {
                        if (pair.second == commandString)
                        {
                            action = pair.first;
                            parsedSuccessfully = true;
                            break;
                        }
                    }
                    if (!parsedSuccessfully)
                    {
                        continue;
                    }
                    // Try parsing the chord
                    Settings::KeyChord chord{ false, false, false, 0 };
                    parsedSuccessfully = false;
                    try
                    {
                        chord = Settings::KeyChord::FromString(keyChordString);
                        parsedSuccessfully = true;
                    }
                    catch (...)
                    {
                        continue;
                    }
                    if (parsedSuccessfully)
                    {
                        newBindings.SetKeyBinding(action, chord);
                    }
                }
            }
        }
        return newBindings;
    }

    // Function Description:
    // - Small helper to insert a given KeyChord, ShortcutAction pair into the
    //   given json array
    // Arguments:
    // - bindingsArray: The JsonArray to insert the object into.
    // - chord: The KeyChord to serailize and place in the json array
    // - actionName: the name of the ShortcutAction to use with this KeyChord
    void _AddShortcutToJsonArray(JsonArray bindingsArray,
                                 const Settings::KeyChord& chord,
                                 const std::wstring& actionName)
    {
        const auto keyString = chord.ToString();
        if (keyString == L"")
        {
            return;
        }

        winrt::Windows::Data::Json::JsonObject jsonObject;
        winrt::Windows::Data::Json::JsonArray keysArray;
        keysArray.Append(JsonValue::CreateStringValue(keyString));
        jsonObject.Insert(KEYS_KEY, keysArray);
        jsonObject.Insert(COMMAND_KEY, JsonValue::CreateStringValue(actionName));

        bindingsArray.Append(jsonObject);
    }

    // Method Description:
    // - Serialize this AppKeyBindings to a json array of objects. Each object
    //   in the array represents a single keybinding, mapping a KeyChord to a
    //   ShortcutAction.
    // Return Value:
    // - a JsonArray which is an equivalent serialization of this object.
    Windows::Data::Json::JsonArray AppKeyBindings::ToJson()
    {
        winrt::Windows::Data::Json::JsonArray bindingsArray;

        // Iterate over all the possible actions in the names list, and see if
        // it has a binding.
        for (auto& actionName : commandNames)
        {
            const auto searchedForAction = actionName.first;
            const auto searchedForName = actionName.second;
            for (const auto& kv : _keyShortcuts)
            {
                const auto chord = kv.first;
                const auto command = kv.second;
                if (command == searchedForAction)
                {
                    _AddShortcutToJsonArray(bindingsArray, chord, searchedForName);
                }
            }
        }

        return bindingsArray;
    }
}
