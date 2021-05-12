#include "pch.h"
#include "ActionArgs.h"
#include "ActionAndArgs.h"
#include "ActionAndArgs.g.cpp"

#include "JsonUtils.h"

#include <LibraryResources.h>

static constexpr std::string_view AdjustFontSizeKey{ "adjustFontSize" };
static constexpr std::string_view CloseOtherTabsKey{ "closeOtherTabs" };
static constexpr std::string_view ClosePaneKey{ "closePane" };
static constexpr std::string_view CloseTabKey{ "closeTab" };
static constexpr std::string_view CloseTabsAfterKey{ "closeTabsAfter" };
static constexpr std::string_view CloseWindowKey{ "closeWindow" };
static constexpr std::string_view CopyTextKey{ "copy" };
static constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
static constexpr std::string_view ExecuteCommandlineKey{ "wt" };
static constexpr std::string_view FindKey{ "find" };
static constexpr std::string_view MoveFocusKey{ "moveFocus" };
static constexpr std::string_view NewTabKey{ "newTab" };
static constexpr std::string_view NextTabKey{ "nextTab" };
static constexpr std::string_view OpenNewTabDropdownKey{ "openNewTabDropdown" };
static constexpr std::string_view OpenSettingsKey{ "openSettings" };
static constexpr std::string_view OpenTabColorPickerKey{ "openTabColorPicker" };
static constexpr std::string_view PasteTextKey{ "paste" };
static constexpr std::string_view PrevTabKey{ "prevTab" };
static constexpr std::string_view RenameTabKey{ "renameTab" };
static constexpr std::string_view OpenTabRenamerKey{ "openTabRenamer" };
static constexpr std::string_view ResetFontSizeKey{ "resetFontSize" };
static constexpr std::string_view ResizePaneKey{ "resizePane" };
static constexpr std::string_view ScrolldownKey{ "scrollDown" };
static constexpr std::string_view ScrolldownpageKey{ "scrollDownPage" };
static constexpr std::string_view ScrollupKey{ "scrollUp" };
static constexpr std::string_view ScrolluppageKey{ "scrollUpPage" };
static constexpr std::string_view ScrollToTopKey{ "scrollToTop" };
static constexpr std::string_view ScrollToBottomKey{ "scrollToBottom" };
static constexpr std::string_view SendInputKey{ "sendInput" };
static constexpr std::string_view SetColorSchemeKey{ "setColorScheme" };
static constexpr std::string_view SetTabColorKey{ "setTabColor" };
static constexpr std::string_view SplitPaneKey{ "splitPane" };
static constexpr std::string_view SwitchToTabKey{ "switchToTab" };
static constexpr std::string_view TabSearchKey{ "tabSearch" };
static constexpr std::string_view ToggleAlwaysOnTopKey{ "toggleAlwaysOnTop" };
static constexpr std::string_view ToggleCommandPaletteKey{ "commandPalette" };
static constexpr std::string_view ToggleFocusModeKey{ "toggleFocusMode" };
static constexpr std::string_view ToggleFullscreenKey{ "toggleFullscreen" };
static constexpr std::string_view TogglePaneZoomKey{ "togglePaneZoom" };
static constexpr std::string_view LegacyToggleRetroEffectKey{ "toggleRetroEffect" };
static constexpr std::string_view ToggleShaderEffectsKey{ "toggleShaderEffects" };
static constexpr std::string_view MoveTabKey{ "moveTab" };
static constexpr std::string_view BreakIntoDebuggerKey{ "breakIntoDebugger" };
static constexpr std::string_view FindMatchKey{ "findMatch" };
static constexpr std::string_view TogglePaneReadOnlyKey{ "toggleReadOnlyMode" };
static constexpr std::string_view NewWindowKey{ "newWindow" };
static constexpr std::string_view IdentifyWindowKey{ "identifyWindow" };
static constexpr std::string_view IdentifyWindowsKey{ "identifyWindows" };
static constexpr std::string_view RenameWindowKey{ "renameWindow" };
static constexpr std::string_view OpenWindowRenamerKey{ "openWindowRenamer" };
static constexpr std::string_view GlobalSummonKey{ "globalSummon" };
static constexpr std::string_view QuakeModeKey{ "quakeMode" };

static constexpr std::string_view ActionKey{ "action" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    using namespace ::Microsoft::Terminal::Settings::Model;

    // Specifically use a map here over an unordered_map. We want to be able to
    // iterate over these entries in-order when we're serializing the keybindings.
    // HERE BE DRAGONS:
    // These are string_views that are being used as keys. These string_views are
    // just pointers to other strings. This could be dangerous, if the map outlived
    // the actual strings being pointed to. However, since both these strings and
    // the map are all const for the lifetime of the app, we have nothing to worry
    // about here.
    const std::map<std::string_view, ShortcutAction, std::less<>> ActionAndArgs::ActionKeyNamesMap{
        { AdjustFontSizeKey, ShortcutAction::AdjustFontSize },
        { CloseOtherTabsKey, ShortcutAction::CloseOtherTabs },
        { ClosePaneKey, ShortcutAction::ClosePane },
        { CloseTabKey, ShortcutAction::CloseTab },
        { CloseTabsAfterKey, ShortcutAction::CloseTabsAfter },
        { CloseWindowKey, ShortcutAction::CloseWindow },
        { CopyTextKey, ShortcutAction::CopyText },
        { DuplicateTabKey, ShortcutAction::DuplicateTab },
        { ExecuteCommandlineKey, ShortcutAction::ExecuteCommandline },
        { FindKey, ShortcutAction::Find },
        { MoveFocusKey, ShortcutAction::MoveFocus },
        { NewTabKey, ShortcutAction::NewTab },
        { NextTabKey, ShortcutAction::NextTab },
        { OpenNewTabDropdownKey, ShortcutAction::OpenNewTabDropdown },
        { OpenSettingsKey, ShortcutAction::OpenSettings },
        { OpenTabColorPickerKey, ShortcutAction::OpenTabColorPicker },
        { PasteTextKey, ShortcutAction::PasteText },
        { PrevTabKey, ShortcutAction::PrevTab },
        { RenameTabKey, ShortcutAction::RenameTab },
        { OpenTabRenamerKey, ShortcutAction::OpenTabRenamer },
        { ResetFontSizeKey, ShortcutAction::ResetFontSize },
        { ResizePaneKey, ShortcutAction::ResizePane },
        { ScrolldownKey, ShortcutAction::ScrollDown },
        { ScrolldownpageKey, ShortcutAction::ScrollDownPage },
        { ScrollupKey, ShortcutAction::ScrollUp },
        { ScrolluppageKey, ShortcutAction::ScrollUpPage },
        { ScrollToTopKey, ShortcutAction::ScrollToTop },
        { ScrollToBottomKey, ShortcutAction::ScrollToBottom },
        { SendInputKey, ShortcutAction::SendInput },
        { SetColorSchemeKey, ShortcutAction::SetColorScheme },
        { SetTabColorKey, ShortcutAction::SetTabColor },
        { SplitPaneKey, ShortcutAction::SplitPane },
        { SwitchToTabKey, ShortcutAction::SwitchToTab },
        { TabSearchKey, ShortcutAction::TabSearch },
        { ToggleAlwaysOnTopKey, ShortcutAction::ToggleAlwaysOnTop },
        { ToggleCommandPaletteKey, ShortcutAction::ToggleCommandPalette },
        { ToggleFocusModeKey, ShortcutAction::ToggleFocusMode },
        { ToggleFullscreenKey, ShortcutAction::ToggleFullscreen },
        { TogglePaneZoomKey, ShortcutAction::TogglePaneZoom },
        { LegacyToggleRetroEffectKey, ShortcutAction::ToggleShaderEffects },
        { ToggleShaderEffectsKey, ShortcutAction::ToggleShaderEffects },
        { MoveTabKey, ShortcutAction::MoveTab },
        { BreakIntoDebuggerKey, ShortcutAction::BreakIntoDebugger },
        { UnboundKey, ShortcutAction::Invalid },
        { FindMatchKey, ShortcutAction::FindMatch },
        { TogglePaneReadOnlyKey, ShortcutAction::TogglePaneReadOnly },
        { NewWindowKey, ShortcutAction::NewWindow },
        { IdentifyWindowKey, ShortcutAction::IdentifyWindow },
        { IdentifyWindowsKey, ShortcutAction::IdentifyWindows },
        { RenameWindowKey, ShortcutAction::RenameWindow },
        { OpenWindowRenamerKey, ShortcutAction::OpenWindowRenamer },
        { GlobalSummonKey, ShortcutAction::GlobalSummon },
        { QuakeModeKey, ShortcutAction::QuakeMode },
    };

    using ParseResult = std::tuple<IActionArgs, std::vector<SettingsLoadWarnings>>;
    using ParseActionFunction = std::function<ParseResult(const Json::Value&)>;

    // This is a map of ShortcutAction->function<IActionArgs(Json::Value)>. It holds
    // a set of deserializer functions that can be used to deserialize a IActionArgs
    // from json. Each type of IActionArgs that can accept arbitrary args should be
    // placed into this map, with the corresponding deserializer function as the
    // value.
    static const std::map<ShortcutAction, ParseActionFunction, std::less<>> argParsers{
        { ShortcutAction::AdjustFontSize, AdjustFontSizeArgs::FromJson },
        { ShortcutAction::CloseOtherTabs, CloseOtherTabsArgs::FromJson },
        { ShortcutAction::CloseTabsAfter, CloseTabsAfterArgs::FromJson },
        { ShortcutAction::CopyText, CopyTextArgs::FromJson },
        { ShortcutAction::ExecuteCommandline, ExecuteCommandlineArgs::FromJson },
        { ShortcutAction::MoveFocus, MoveFocusArgs::FromJson },
        { ShortcutAction::NewTab, NewTabArgs::FromJson },
        { ShortcutAction::OpenSettings, OpenSettingsArgs::FromJson },
        { ShortcutAction::RenameTab, RenameTabArgs::FromJson },
        { ShortcutAction::ResizePane, ResizePaneArgs::FromJson },
        { ShortcutAction::SendInput, SendInputArgs::FromJson },
        { ShortcutAction::SetColorScheme, SetColorSchemeArgs::FromJson },
        { ShortcutAction::SetTabColor, SetTabColorArgs::FromJson },
        { ShortcutAction::SplitPane, SplitPaneArgs::FromJson },
        { ShortcutAction::SwitchToTab, SwitchToTabArgs::FromJson },
        { ShortcutAction::ScrollUp, ScrollUpArgs::FromJson },
        { ShortcutAction::ScrollDown, ScrollDownArgs::FromJson },
        { ShortcutAction::MoveTab, MoveTabArgs::FromJson },
        { ShortcutAction::ToggleCommandPalette, ToggleCommandPaletteArgs::FromJson },
        { ShortcutAction::FindMatch, FindMatchArgs::FromJson },
        { ShortcutAction::NewWindow, NewWindowArgs::FromJson },
        { ShortcutAction::PrevTab, PrevTabArgs::FromJson },
        { ShortcutAction::NextTab, NextTabArgs::FromJson },
        { ShortcutAction::RenameWindow, RenameWindowArgs::FromJson },
        { ShortcutAction::GlobalSummon, GlobalSummonArgs::FromJson },
        { ShortcutAction::QuakeMode, GlobalSummonArgs::QuakeModeFromJson },

        { ShortcutAction::Invalid, nullptr },
    };

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
        const auto found = ActionAndArgs::ActionKeyNamesMap.find(actionString);
        return found != ActionAndArgs::ActionKeyNamesMap.end() ? found->second : ShortcutAction::Invalid;
    }

    // Method Description:
    // - Deserialize an ActionAndArgs from the provided json object or string `json`.
    //   * If json is a string, we'll attempt to treat it as an action name,
    //     without arguments.
    //   * If json is an object, we'll attempt to retrieve the action name from
    //     its "action" property, and we'll use that name to fine a deserializer
    //     to precess the rest of the arguments in the json object.
    // - If the action name is null or "unbound", or we don't understand the
    //   action name, or we failed to parse the arguments to this action, we'll
    //   return null. This should indicate to the caller that the action should
    //   be unbound.
    // - If there were any warnings while parsing arguments for the action,
    //   they'll be appended to the warnings parameter.
    // Arguments:
    // - json: The Json::Value to attempt to parse as an ActionAndArgs
    // - warnings: If there were any warnings during parsing, they'll be
    //   appended to this vector.
    // Return Value:
    // - a deserialized ActionAndArgs corresponding to the values in json, or
    //   an "invalid" action if we failed to deserialize an action.
    winrt::com_ptr<ActionAndArgs> ActionAndArgs::FromJson(const Json::Value& json,
                                                          std::vector<SettingsLoadWarnings>& warnings)
    {
        // Invalid is our placeholder that the action was not parsed.
        ShortcutAction action = ShortcutAction::Invalid;

        // Actions can be serialized in two styles:
        //   "action": "switchToTab0",
        //   "action": { "action": "switchToTab", "index": 0 },
        // NOTE: For keybindings, the "action" param is actually "command"

        // 1. In the first case, the json is a string, that's the
        //    action name. There are no provided args, so we'll pass
        //    Json::Value::null to the parse function.
        // 2. In the second case, the json is an object. We'll use the
        //    "action" in that object as the action name. We'll then pass
        //    the json object to the arg parser, for further parsing.

        auto argsVal = Json::Value::null;

        // Only try to parse the action if it's actually a string value.
        // `null` will not pass this check.
        if (json.isString())
        {
            auto commandString = json.asString();
            action = GetActionFromString(commandString);
        }
        else if (json.isObject())
        {
            if (const auto actionString{ JsonUtils::GetValueForKey<std::optional<std::string>>(json, ActionKey) })
            {
                action = GetActionFromString(*actionString);
                argsVal = json;
            }
        }

        // Some keybindings can accept other arbitrary arguments. If it
        // does, we'll try to deserialize any "args" that were provided with
        // the binding.
        IActionArgs args{ nullptr };
        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> parseWarnings;
        const auto deserializersIter = argParsers.find(action);
        if (deserializersIter != argParsers.end())
        {
            auto pfn = deserializersIter->second;
            if (pfn)
            {
                std::tie(args, parseWarnings) = pfn(argsVal);
            }
            warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());

            // if an arg parser was registered, but failed, bail
            if (pfn && args == nullptr)
            {
                return make_self<ActionAndArgs>();
            }
        }

        // Something like
        //      { name: "foo", action: "unbound" }
        // will _remove_ the "foo" command, by returning an "invalid" action here.
        return make_self<ActionAndArgs>(action, args);
    }

    com_ptr<ActionAndArgs> ActionAndArgs::Copy() const
    {
        auto copy{ winrt::make_self<ActionAndArgs>() };
        copy->_Action = _Action;
        copy->_Args = _Args ? _Args.Copy() : IActionArgs{ nullptr };
        return copy;
    }

    winrt::hstring ActionAndArgs::GenerateName() const
    {
        // Use a magic static to initialize this map, because we won't be able
        // to load the resources at _init_, only at runtime.
        static const auto GeneratedActionNames = []() {
            return std::unordered_map<ShortcutAction, winrt::hstring>{
                { ShortcutAction::AdjustFontSize, RS_(L"AdjustFontSizeCommandKey") },
                { ShortcutAction::CloseOtherTabs, L"" }, // Intentionally omitted, must be generated by GenerateName
                { ShortcutAction::ClosePane, RS_(L"ClosePaneCommandKey") },
                { ShortcutAction::CloseTab, RS_(L"CloseTabCommandKey") },
                { ShortcutAction::CloseTabsAfter, L"" }, // Intentionally omitted, must be generated by GenerateName
                { ShortcutAction::CloseWindow, RS_(L"CloseWindowCommandKey") },
                { ShortcutAction::CopyText, RS_(L"CopyTextCommandKey") },
                { ShortcutAction::DuplicateTab, RS_(L"DuplicateTabCommandKey") },
                { ShortcutAction::ExecuteCommandline, RS_(L"ExecuteCommandlineCommandKey") },
                { ShortcutAction::Find, RS_(L"FindCommandKey") },
                { ShortcutAction::Invalid, L"" },
                { ShortcutAction::MoveFocus, RS_(L"MoveFocusCommandKey") },
                { ShortcutAction::NewTab, RS_(L"NewTabCommandKey") },
                { ShortcutAction::NextTab, RS_(L"NextTabCommandKey") },
                { ShortcutAction::OpenNewTabDropdown, RS_(L"OpenNewTabDropdownCommandKey") },
                { ShortcutAction::OpenSettings, RS_(L"OpenSettingsUICommandKey") },
                { ShortcutAction::OpenTabColorPicker, RS_(L"OpenTabColorPickerCommandKey") },
                { ShortcutAction::PasteText, RS_(L"PasteTextCommandKey") },
                { ShortcutAction::PrevTab, RS_(L"PrevTabCommandKey") },
                { ShortcutAction::RenameTab, RS_(L"ResetTabNameCommandKey") },
                { ShortcutAction::OpenTabRenamer, RS_(L"OpenTabRenamerCommandKey") },
                { ShortcutAction::ResetFontSize, RS_(L"ResetFontSizeCommandKey") },
                { ShortcutAction::ResizePane, RS_(L"ResizePaneCommandKey") },
                { ShortcutAction::ScrollDown, RS_(L"ScrollDownCommandKey") },
                { ShortcutAction::ScrollDownPage, RS_(L"ScrollDownPageCommandKey") },
                { ShortcutAction::ScrollUp, RS_(L"ScrollUpCommandKey") },
                { ShortcutAction::ScrollUpPage, RS_(L"ScrollUpPageCommandKey") },
                { ShortcutAction::ScrollToTop, RS_(L"ScrollToTopCommandKey") },
                { ShortcutAction::ScrollToBottom, RS_(L"ScrollToBottomCommandKey") },
                { ShortcutAction::SendInput, L"" },
                { ShortcutAction::SetColorScheme, L"" },
                { ShortcutAction::SetTabColor, RS_(L"ResetTabColorCommandKey") },
                { ShortcutAction::SplitPane, RS_(L"SplitPaneCommandKey") },
                { ShortcutAction::SwitchToTab, RS_(L"SwitchToTabCommandKey") },
                { ShortcutAction::TabSearch, RS_(L"TabSearchCommandKey") },
                { ShortcutAction::ToggleAlwaysOnTop, RS_(L"ToggleAlwaysOnTopCommandKey") },
                { ShortcutAction::ToggleCommandPalette, L"" },
                { ShortcutAction::ToggleFocusMode, RS_(L"ToggleFocusModeCommandKey") },
                { ShortcutAction::ToggleFullscreen, RS_(L"ToggleFullscreenCommandKey") },
                { ShortcutAction::TogglePaneZoom, RS_(L"TogglePaneZoomCommandKey") },
                { ShortcutAction::ToggleShaderEffects, RS_(L"ToggleShaderEffectsCommandKey") },
                { ShortcutAction::MoveTab, L"" }, // Intentionally omitted, must be generated by GenerateName
                { ShortcutAction::BreakIntoDebugger, RS_(L"BreakIntoDebuggerCommandKey") },
                { ShortcutAction::FindMatch, L"" }, // Intentionally omitted, must be generated by GenerateName
                { ShortcutAction::TogglePaneReadOnly, RS_(L"TogglePaneReadOnlyCommandKey") },
                { ShortcutAction::NewWindow, RS_(L"NewWindowCommandKey") },
                { ShortcutAction::IdentifyWindow, RS_(L"IdentifyWindowCommandKey") },
                { ShortcutAction::IdentifyWindows, RS_(L"IdentifyWindowsCommandKey") },
                { ShortcutAction::RenameWindow, RS_(L"ResetWindowNameCommandKey") },
                { ShortcutAction::OpenWindowRenamer, RS_(L"OpenWindowRenamerCommandKey") },
                { ShortcutAction::GlobalSummon, L"" }, // Intentionally omitted, must be generated by GenerateName
                { ShortcutAction::QuakeMode, RS_(L"QuakeModeCommandKey") },
            };
        }();

        if (_Args)
        {
            auto nameFromArgs = _Args.GenerateName();
            if (!nameFromArgs.empty())
            {
                return nameFromArgs;
            }
        }

        const auto found = GeneratedActionNames.find(_Action);
        return found != GeneratedActionNames.end() ? found->second : L"";
    }
}
