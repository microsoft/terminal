// Copyright (c) Microsoft Corporation.
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
    { ShortcutAction::SwitchToTab, SWITCHTOTAB_KEY },
    { ShortcutAction::NextTab, NEXTTAB_KEY },
    { ShortcutAction::PrevTab, PREVTAB_KEY },
    { ShortcutAction::IncreaseFontSize, INCREASEFONTSIZE_KEY },
    { ShortcutAction::DecreaseFontSize, DECREASEFONTSIZE_KEY },
    { ShortcutAction::ScrollUp, SCROLLUP_KEY },
    { ShortcutAction::ScrollDown, SCROLLDOWN_KEY },
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

            case ShortcutAction::NextTab:
                _NextTabHandlers();
                return true;
            case ShortcutAction::PrevTab:
                _PrevTabHandlers();
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


    TerminalApp::AppKeyBindings AppKeyBindings::FromJson(Windows::Data::Json::JsonArray const& json)
    {
        throw hresult_not_implemented();
    }
    Windows::Data::Json::JsonArray AppKeyBindings::ToJson()
    {
        winrt::Windows::Data::Json::JsonArray bindingsArray;

        for (const auto& kv : _keyShortcuts)
        {
            const auto chord = kv.first;
            const auto command = kv.second;

            const auto keyString = chord.ToString();
            if (keyString == L"")
            {
                continue;
            }

            std::wstring commandName{ L"" };
            for (const auto& pair : commandNames)
            {
                if (pair.first == command)
                {
                    commandName = pair.second;
                }
            }
            if (commandName == L"")
            {
                continue;
            }


            winrt::Windows::Data::Json::JsonObject jsonObject;
            winrt::Windows::Data::Json::JsonArray keysArray;
            keysArray.Append(JsonValue::CreateStringValue(keyString));
            jsonObject.Insert(KEYS_KEY, keysArray);
            jsonObject.Insert(COMMAND_KEY, JsonValue::CreateStringValue(commandName));

            bindingsArray.Append(jsonObject);
        }

        return bindingsArray;
    }


}
