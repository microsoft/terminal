// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppKeyBindings.g.h"
#include "..\inc\cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct KeyChordHash
    {
        std::size_t operator()(const winrt::Microsoft::Terminal::Settings::KeyChord& key) const
        {
            std::hash<int32_t> keyHash;
            std::hash<winrt::Microsoft::Terminal::Settings::KeyModifiers> modifiersHash;
            std::size_t hashedKey = keyHash(key.Vkey());
            std::size_t hashedMods = modifiersHash(key.Modifiers());
            return hashedKey ^ hashedMods;
        }
    };

    struct KeyChordEquality
    {
        bool operator()(const winrt::Microsoft::Terminal::Settings::KeyChord& lhs, const winrt::Microsoft::Terminal::Settings::KeyChord& rhs) const
        {
            return lhs.Modifiers() == rhs.Modifiers() && lhs.Vkey() == rhs.Vkey();
        }
    };

    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings>
    {
        AppKeyBindings() = default;

        bool TryKeyChord(winrt::Microsoft::Terminal::Settings::KeyChord const& kc);
        void SetKeyBinding(TerminalApp::ShortcutAction const& action, winrt::Microsoft::Terminal::Settings::KeyChord const& chord);
        Microsoft::Terminal::Settings::KeyChord GetKeyBinding(TerminalApp::ShortcutAction const& action);

        static Windows::System::VirtualKeyModifiers ConvertVKModifiers(winrt::Microsoft::Terminal::Settings::KeyModifiers modifiers);

        // clang-format off
        DECLARE_EVENT(CopyText,          _CopyTextHandlers,          TerminalApp::CopyTextEventArgs);
        DECLARE_EVENT(PasteText,         _PasteTextHandlers,         TerminalApp::PasteTextEventArgs);
        DECLARE_EVENT(NewTab,            _NewTabHandlers,            TerminalApp::NewTabEventArgs);
        DECLARE_EVENT(DuplicateTab,      _DuplicateTabHandlers,      TerminalApp::DuplicateTabEventArgs);
        DECLARE_EVENT(NewTabWithProfile, _NewTabWithProfileHandlers, TerminalApp::NewTabWithProfileEventArgs);
        DECLARE_EVENT(NewWindow,         _NewWindowHandlers,         TerminalApp::NewWindowEventArgs);
        DECLARE_EVENT(CloseWindow,       _CloseWindowHandlers,       TerminalApp::CloseWindowEventArgs);
        DECLARE_EVENT(CloseTab,          _CloseTabHandlers,          TerminalApp::CloseTabEventArgs);
        DECLARE_EVENT(ClosePane,         _ClosePaneHandlers,         TerminalApp::ClosePaneEventArgs);
        DECLARE_EVENT(SwitchToTab,       _SwitchToTabHandlers,       TerminalApp::SwitchToTabEventArgs);
        DECLARE_EVENT(NextTab,           _NextTabHandlers,           TerminalApp::NextTabEventArgs);
        DECLARE_EVENT(PrevTab,           _PrevTabHandlers,           TerminalApp::PrevTabEventArgs);
        DECLARE_EVENT(SplitVertical,     _SplitVerticalHandlers,     TerminalApp::SplitVerticalEventArgs);
        DECLARE_EVENT(SplitHorizontal,   _SplitHorizontalHandlers,   TerminalApp::SplitHorizontalEventArgs);
        DECLARE_EVENT(IncreaseFontSize,  _IncreaseFontSizeHandlers,  TerminalApp::IncreaseFontSizeEventArgs);
        DECLARE_EVENT(DecreaseFontSize,  _DecreaseFontSizeHandlers,  TerminalApp::DecreaseFontSizeEventArgs);
        DECLARE_EVENT(ScrollUp,          _ScrollUpHandlers,          TerminalApp::ScrollUpEventArgs);
        DECLARE_EVENT(ScrollDown,        _ScrollDownHandlers,        TerminalApp::ScrollDownEventArgs);
        DECLARE_EVENT(ScrollUpPage,      _ScrollUpPageHandlers,      TerminalApp::ScrollUpPageEventArgs);
        DECLARE_EVENT(ScrollDownPage,    _ScrollDownPageHandlers,    TerminalApp::ScrollDownPageEventArgs);
        DECLARE_EVENT(OpenSettings,      _OpenSettingsHandlers,      TerminalApp::OpenSettingsEventArgs);
        DECLARE_EVENT(ResizePane,        _ResizePaneHandlers,        TerminalApp::ResizePaneEventArgs);
        DECLARE_EVENT(MoveFocus,         _MoveFocusHandlers,         TerminalApp::MoveFocusEventArgs);
        // clang-format on

    private:
        std::unordered_map<winrt::Microsoft::Terminal::Settings::KeyChord, TerminalApp::ShortcutAction, KeyChordHash, KeyChordEquality> _keyShortcuts;
        bool _DoAction(ShortcutAction action);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct AppKeyBindings : AppKeyBindingsT<AppKeyBindings, implementation::AppKeyBindings>
    {
    };
}
