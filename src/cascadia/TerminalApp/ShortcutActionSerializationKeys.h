// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

constexpr std::string_view CopyTextKey{ "copy" };
constexpr std::string_view CopyTextWithoutNewlinesKey{ "copyTextWithoutNewlines" };
constexpr std::string_view PasteTextKey{ "paste" };
constexpr std::string_view NewTabKey{ "newTab" };
constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
constexpr std::string_view NewTabWithProfile0Key{ "newTabProfile0" };
constexpr std::string_view NewTabWithProfile1Key{ "newTabProfile1" };
constexpr std::string_view NewTabWithProfile2Key{ "newTabProfile2" };
constexpr std::string_view NewTabWithProfile3Key{ "newTabProfile3" };
constexpr std::string_view NewTabWithProfile4Key{ "newTabProfile4" };
constexpr std::string_view NewTabWithProfile5Key{ "newTabProfile5" };
constexpr std::string_view NewTabWithProfile6Key{ "newTabProfile6" };
constexpr std::string_view NewTabWithProfile7Key{ "newTabProfile7" };
constexpr std::string_view NewTabWithProfile8Key{ "newTabProfile8" };
constexpr std::string_view NewWindowKey{ "newWindow" };
constexpr std::string_view CloseWindowKey{ "closeWindow" };
constexpr std::string_view CloseTabKey{ "closeTab" };
constexpr std::string_view ClosePaneKey{ "closePane" };
constexpr std::string_view SwitchtoTabKey{ "switchToTab" };
constexpr std::string_view NextTabKey{ "nextTab" };
constexpr std::string_view PrevTabKey{ "prevTab" };
constexpr std::string_view IncreaseFontSizeKey{ "increaseFontSize" };
constexpr std::string_view DecreaseFontSizeKey{ "decreaseFontSize" };
constexpr std::string_view ScrollupKey{ "scrollUp" };
constexpr std::string_view ScrolldownKey{ "scrollDown" };
constexpr std::string_view ScrolluppageKey{ "scrollUpPage" };
constexpr std::string_view ScrolldownpageKey{ "scrollDownPage" };
constexpr std::string_view SwitchToTab0Key{ "switchToTab0" };
constexpr std::string_view SwitchToTab1Key{ "switchToTab1" };
constexpr std::string_view SwitchToTab2Key{ "switchToTab2" };
constexpr std::string_view SwitchToTab3Key{ "switchToTab3" };
constexpr std::string_view SwitchToTab4Key{ "switchToTab4" };
constexpr std::string_view SwitchToTab5Key{ "switchToTab5" };
constexpr std::string_view SwitchToTab6Key{ "switchToTab6" };
constexpr std::string_view SwitchToTab7Key{ "switchToTab7" };
constexpr std::string_view SwitchToTab8Key{ "switchToTab8" };
constexpr std::string_view OpenSettingsKey{ "openSettings" };
constexpr std::string_view SplitHorizontalKey{ "splitHorizontal" };
constexpr std::string_view SplitVerticalKey{ "splitVertical" };
constexpr std::string_view ResizePaneLeftKey{ "resizePaneLeft" };
constexpr std::string_view ResizePaneRightKey{ "resizePaneRight" };
constexpr std::string_view ResizePaneUpKey{ "resizePaneUp" };
constexpr std::string_view ResizePaneDownKey{ "resizePaneDown" };
constexpr std::string_view MoveFocusLeftKey{ "moveFocusLeft" };
constexpr std::string_view MoveFocusRightKey{ "moveFocusRight" };
constexpr std::string_view MoveFocusUpKey{ "moveFocusUp" };
constexpr std::string_view MoveFocusDownKey{ "moveFocusDown" };
constexpr std::string_view ToggleCommandPaletteKey{ "toggleCommandPalette" };

// Specifically use a map here over an unordered_map. We want to be able to
// iterate over these entries in-order when we're serializing the keybindings.
// HERE BE DRAGONS:
// These are string_views that are being used as keys. These string_views are
// just pointers to other strings. This could be dangerous, if the map outlived
// the actual strings being pointed to. However, since both these strings and
// the map are all const for the lifetime of the app, we have nothing to worry
// about here.
const std::map<std::string_view, winrt::TerminalApp::ShortcutAction, std::less<>> commandNames{
    { CopyTextKey, winrt::TerminalApp::ShortcutAction::CopyText },
    { CopyTextWithoutNewlinesKey, winrt::TerminalApp::ShortcutAction::CopyTextWithoutNewlines },
    { PasteTextKey, winrt::TerminalApp::ShortcutAction::PasteText },
    { NewTabKey, winrt::TerminalApp::ShortcutAction::NewTab },
    { DuplicateTabKey, winrt::TerminalApp::ShortcutAction::DuplicateTab },
    { NewTabWithProfile0Key, winrt::TerminalApp::ShortcutAction::NewTabProfile0 },
    { NewTabWithProfile1Key, winrt::TerminalApp::ShortcutAction::NewTabProfile1 },
    { NewTabWithProfile2Key, winrt::TerminalApp::ShortcutAction::NewTabProfile2 },
    { NewTabWithProfile3Key, winrt::TerminalApp::ShortcutAction::NewTabProfile3 },
    { NewTabWithProfile4Key, winrt::TerminalApp::ShortcutAction::NewTabProfile4 },
    { NewTabWithProfile5Key, winrt::TerminalApp::ShortcutAction::NewTabProfile5 },
    { NewTabWithProfile6Key, winrt::TerminalApp::ShortcutAction::NewTabProfile6 },
    { NewTabWithProfile7Key, winrt::TerminalApp::ShortcutAction::NewTabProfile7 },
    { NewTabWithProfile8Key, winrt::TerminalApp::ShortcutAction::NewTabProfile8 },
    { NewWindowKey, winrt::TerminalApp::ShortcutAction::NewWindow },
    { CloseWindowKey, winrt::TerminalApp::ShortcutAction::CloseWindow },
    { CloseTabKey, winrt::TerminalApp::ShortcutAction::CloseTab },
    { ClosePaneKey, winrt::TerminalApp::ShortcutAction::ClosePane },
    { NextTabKey, winrt::TerminalApp::ShortcutAction::NextTab },
    { PrevTabKey, winrt::TerminalApp::ShortcutAction::PrevTab },
    { IncreaseFontSizeKey, winrt::TerminalApp::ShortcutAction::IncreaseFontSize },
    { DecreaseFontSizeKey, winrt::TerminalApp::ShortcutAction::DecreaseFontSize },
    { ScrollupKey, winrt::TerminalApp::ShortcutAction::ScrollUp },
    { ScrolldownKey, winrt::TerminalApp::ShortcutAction::ScrollDown },
    { ScrolluppageKey, winrt::TerminalApp::ShortcutAction::ScrollUpPage },
    { ScrolldownpageKey, winrt::TerminalApp::ShortcutAction::ScrollDownPage },
    { SwitchToTab0Key, winrt::TerminalApp::ShortcutAction::SwitchToTab0 },
    { SwitchToTab1Key, winrt::TerminalApp::ShortcutAction::SwitchToTab1 },
    { SwitchToTab2Key, winrt::TerminalApp::ShortcutAction::SwitchToTab2 },
    { SwitchToTab3Key, winrt::TerminalApp::ShortcutAction::SwitchToTab3 },
    { SwitchToTab4Key, winrt::TerminalApp::ShortcutAction::SwitchToTab4 },
    { SwitchToTab5Key, winrt::TerminalApp::ShortcutAction::SwitchToTab5 },
    { SwitchToTab6Key, winrt::TerminalApp::ShortcutAction::SwitchToTab6 },
    { SwitchToTab7Key, winrt::TerminalApp::ShortcutAction::SwitchToTab7 },
    { SwitchToTab8Key, winrt::TerminalApp::ShortcutAction::SwitchToTab8 },
    { SplitHorizontalKey, winrt::TerminalApp::ShortcutAction::SplitHorizontal },
    { SplitVerticalKey, winrt::TerminalApp::ShortcutAction::SplitVertical },
    { ResizePaneLeftKey, winrt::TerminalApp::ShortcutAction::ResizePaneLeft },
    { ResizePaneRightKey, winrt::TerminalApp::ShortcutAction::ResizePaneRight },
    { ResizePaneUpKey, winrt::TerminalApp::ShortcutAction::ResizePaneUp },
    { ResizePaneDownKey, winrt::TerminalApp::ShortcutAction::ResizePaneDown },
    { MoveFocusLeftKey, winrt::TerminalApp::ShortcutAction::MoveFocusLeft },
    { MoveFocusRightKey, winrt::TerminalApp::ShortcutAction::MoveFocusRight },
    { MoveFocusUpKey, winrt::TerminalApp::ShortcutAction::MoveFocusUp },
    { MoveFocusDownKey, winrt::TerminalApp::ShortcutAction::MoveFocusDown },
    { OpenSettingsKey, winrt::TerminalApp::ShortcutAction::OpenSettings },
    { ToggleCommandPaletteKey, winrt::TerminalApp::ShortcutAction::ToggleCommandPalette },
};
