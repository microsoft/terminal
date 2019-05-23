// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppKeyBindings.h"
#include "KeyChordSerialization.h"

#include "AppKeyBindings.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

static constexpr std::wstring_view KEYS_KEY{ L"keys" };
static constexpr std::wstring_view COMMAND_KEY{ L"command" };

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
    { OPENSETTINGS_KEY, ShortcutAction::OpenSettings },
};

namespace winrt::TerminalApp::implementation
{
    void AppKeyBindings::SetKeyBinding(const TerminalApp::ShortcutAction& action,
                                       const Settings::KeyChord& chord)
    {
        _keyShortcuts[chord] = action;
    }

    Microsoft::Terminal::Settings::KeyChord AppKeyBindings::GetKeyBinding(TerminalApp::ShortcutAction const& action)
    {
        for (auto& kv : _keyShortcuts)
        {
            if (kv.second == action) return kv.first;
        }
        return { nullptr };
    }

    bool AppKeyBindings::TryKeyChord(const Settings::KeyChord& kc)
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

            default:
                return false;
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
    //   both a `command` string and a `keys` array, where `command` is one of
    //   the names listed in `commandNames`, and `keys` is an array of
    //   keypresses. Currently, the array should contain a single string, which
    //   can be deserialized into a KeyChord.
    // Arguments:
    // - json: and array of JsonObject's to deserialize into our _keyShortcuts mapping.
    // Return Value:
    // - the newly constructed AppKeyBindings object.
    TerminalApp::AppKeyBindings AppKeyBindings::FromJson(const JsonArray& json)
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

                    // Try matching the command to one we have
                    auto found = commandNames.find(commandString);
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

    // Function Description:
    // - Small helper to insert a given KeyChord, ShortcutAction pair into the
    //   given json array
    // Arguments:
    // - bindingsArray: The JsonArray to insert the object into.
    // - chord: The KeyChord to serailize and place in the json array
    // - actionName: the name of the ShortcutAction to use with this KeyChord
    static void _AddShortcutToJsonArray(const JsonArray& bindingsArray,
                                        const Settings::KeyChord& chord,
                                        const std::wstring_view& actionName)
    {
        const auto keyString = KeyChordSerialization::ToString(chord);
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
            const auto searchedForName = actionName.first;
            const auto searchedForAction = actionName.second;
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

    // Method Description:
    // - Takes the KeyModifier flags from Terminal and maps them to the WinRT types which are used by XAML
    // Return Value:
    // - a Windows::System::VirtualKeyModifiers object with the flags of which modifiers used.
    Windows::System::VirtualKeyModifiers AppKeyBindings::ConvertVKModifiers(Settings::KeyModifiers modifiers)
    {
        Windows::System::VirtualKeyModifiers keyModifiers = Windows::System::VirtualKeyModifiers::None;
        
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Ctrl))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Control;
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Shift))
        {
            keyModifiers |= Windows::System::VirtualKeyModifiers::Shift;
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Alt))
        {
            // note: Menu is the Alt VK_MENU
            keyModifiers |= Windows::System::VirtualKeyModifiers::Menu;
        }

        return keyModifiers;
    }

    // Method Description:
    // - Handles the special case of providing a text override for the UI shortcut due to VK_OEM_COMMA issue.
    //      Looks at the flags from the KeyChord modifiers and provides a concatenated string value of all
    //      in the same order that XAML would put them as well.
    // Return Value:
    // - a WinRT hstring representation of the key modifiers for the shortcut
    //NOTE: This needs to be localized with https://github.com/microsoft/terminal/issues/794 if XAML framework issue not resolved before then
    winrt::hstring AppKeyBindings::FormatOverrideShortcutText(Settings::KeyModifiers modifiers)
    {
        std::wstring buffer{ L"" };

        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Ctrl))
        {
            buffer += L"Ctrl+";
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }
        if (WI_IsFlagSet(modifiers, Settings::KeyModifiers::Alt))
        {
            buffer += L"Alt+";
        }

        return winrt::hstring{ buffer };
    }
}
