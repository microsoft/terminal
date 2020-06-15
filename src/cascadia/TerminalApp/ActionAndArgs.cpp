#include "pch.h"
#include "ActionArgs.h"
#include "ActionAndArgs.h"
#include "ActionAndArgs.g.cpp"

static constexpr std::string_view ActionKey{ "action" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

static constexpr std::string_view CopyTextKey{ "copy" };
static constexpr std::string_view PasteTextKey{ "paste" };
static constexpr std::string_view OpenNewTabDropdownKey{ "openNewTabDropdown" };
static constexpr std::string_view DuplicateTabKey{ "duplicateTab" };
static constexpr std::string_view NewTabKey{ "newTab" };
static constexpr std::string_view NewWindowKey{ "newWindow" };
static constexpr std::string_view CloseWindowKey{ "closeWindow" };
static constexpr std::string_view CloseTabKey{ "closeTab" };
static constexpr std::string_view ClosePaneKey{ "closePane" };
static constexpr std::string_view SwitchtoTabKey{ "switchToTab" };
static constexpr std::string_view NextTabKey{ "nextTab" };
static constexpr std::string_view PrevTabKey{ "prevTab" };
static constexpr std::string_view AdjustFontSizeKey{ "adjustFontSize" };
static constexpr std::string_view ResetFontSizeKey{ "resetFontSize" };
static constexpr std::string_view ScrollupKey{ "scrollUp" };
static constexpr std::string_view ScrolldownKey{ "scrollDown" };
static constexpr std::string_view ScrolluppageKey{ "scrollUpPage" };
static constexpr std::string_view ScrolldownpageKey{ "scrollDownPage" };
static constexpr std::string_view SwitchToTabKey{ "switchToTab" };
static constexpr std::string_view OpenSettingsKey{ "openSettings" }; // TODO GH#2557: Add args for OpenSettings
static constexpr std::string_view SplitPaneKey{ "splitPane" };
static constexpr std::string_view ResizePaneKey{ "resizePane" };
static constexpr std::string_view MoveFocusKey{ "moveFocus" };
static constexpr std::string_view FindKey{ "find" };
static constexpr std::string_view ToggleFullscreenKey{ "toggleFullscreen" };

namespace winrt::TerminalApp::implementation
{
    // Specifically use a map here over an unordered_map. We want to be able to
    // iterate over these entries in-order when we're serializing the keybindings.
    // HERE BE DRAGONS:
    // These are string_views that are being used as keys. These string_views are
    // just pointers to other strings. This could be dangerous, if the map outlived
    // the actual strings being pointed to. However, since both these strings and
    // the map are all const for the lifetime of the app, we have nothing to worry
    // about here.
    const std::map<std::string_view, ShortcutAction, std::less<>> ActionAndArgs::ActionNamesMap{
        { CopyTextKey, ShortcutAction::CopyText },
        { PasteTextKey, ShortcutAction::PasteText },
        { OpenNewTabDropdownKey, ShortcutAction::OpenNewTabDropdown },
        { DuplicateTabKey, ShortcutAction::DuplicateTab },
        { NewTabKey, ShortcutAction::NewTab },
        { NewWindowKey, ShortcutAction::NewWindow },
        { CloseWindowKey, ShortcutAction::CloseWindow },
        { CloseTabKey, ShortcutAction::CloseTab },
        { ClosePaneKey, ShortcutAction::ClosePane },
        { NextTabKey, ShortcutAction::NextTab },
        { PrevTabKey, ShortcutAction::PrevTab },
        { AdjustFontSizeKey, ShortcutAction::AdjustFontSize },
        { ResetFontSizeKey, ShortcutAction::ResetFontSize },
        { ScrollupKey, ShortcutAction::ScrollUp },
        { ScrolldownKey, ShortcutAction::ScrollDown },
        { ScrolluppageKey, ShortcutAction::ScrollUpPage },
        { ScrolldownpageKey, ShortcutAction::ScrollDownPage },
        { SwitchToTabKey, ShortcutAction::SwitchToTab },
        { ResizePaneKey, ShortcutAction::ResizePane },
        { MoveFocusKey, ShortcutAction::MoveFocus },
        { OpenSettingsKey, ShortcutAction::OpenSettings },
        { ToggleFullscreenKey, ShortcutAction::ToggleFullscreen },
        { SplitPaneKey, ShortcutAction::SplitPane },
        { UnboundKey, ShortcutAction::Invalid },
        { FindKey, ShortcutAction::Find }
    };

    using ParseResult = std::tuple<IActionArgs, std::vector<::TerminalApp::SettingsLoadWarnings>>;
    using ParseActionFunction = std::function<ParseResult(const Json::Value&)>;

    // This is a map of ShortcutAction->function<IActionArgs(Json::Value)>. It holds
    // a set of deserializer functions that can be used to deserialize a IActionArgs
    // from json. Each type of IActionArgs that can accept arbitrary args should be
    // placed into this map, with the corresponding deserializer function as the
    // value.
    static const std::map<ShortcutAction, ParseActionFunction, std::less<>> argParsers{
        { ShortcutAction::CopyText, winrt::TerminalApp::implementation::CopyTextArgs::FromJson },

        { ShortcutAction::NewTab, winrt::TerminalApp::implementation::NewTabArgs::FromJson },

        { ShortcutAction::SwitchToTab, winrt::TerminalApp::implementation::SwitchToTabArgs::FromJson },

        { ShortcutAction::ResizePane, winrt::TerminalApp::implementation::ResizePaneArgs::FromJson },

        { ShortcutAction::MoveFocus, winrt::TerminalApp::implementation::MoveFocusArgs::FromJson },

        { ShortcutAction::AdjustFontSize, winrt::TerminalApp::implementation::AdjustFontSizeArgs::FromJson },

        { ShortcutAction::SplitPane, winrt::TerminalApp::implementation::SplitPaneArgs::FromJson },

        { ShortcutAction::OpenSettings, winrt::TerminalApp::implementation::OpenSettingsArgs::FromJson },

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
        const auto found = ActionAndArgs::ActionNamesMap.find(actionString);
        return found != ActionAndArgs::ActionNamesMap.end() ? found->second : ShortcutAction::Invalid;
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
    //   null if we failed to deserialize an action.
    winrt::com_ptr<ActionAndArgs> ActionAndArgs::FromJson(const Json::Value& json,
                                                          std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
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
            const auto actionVal = json[JsonKey(ActionKey)];
            if (actionVal.isString())
            {
                auto actionString = actionVal.asString();
                action = GetActionFromString(actionString);
                argsVal = json;
            }
        }

        // Some keybindings can accept other arbitrary arguments. If it
        // does, we'll try to deserialize any "args" that were provided with
        // the binding.
        IActionArgs args{ nullptr };
        std::vector<::TerminalApp::SettingsLoadWarnings> parseWarnings;
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
                return nullptr;
            }
        }

        if (action != ShortcutAction::Invalid)
        {
            auto actionAndArgs = winrt::make_self<ActionAndArgs>();
            actionAndArgs->Action(action);
            actionAndArgs->Args(args);

            return actionAndArgs;
        }
        else
        {
            return nullptr;
        }
    }

}
