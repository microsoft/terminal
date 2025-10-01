#include "pch.h"
#include "AllShortcutActions.h"
#include "ActionArgs.h"
#include "ActionAndArgs.h"
#include "ActionAndArgs.g.cpp"

#include "JsonUtils.h"
#include "HashUtils.h"

#include <LibraryResources.h>
#include <til/static_map.h>
#include <ScopedResourceLoader.h>

static constexpr std::string_view AdjustFontSizeKey{ "adjustFontSize" };
static constexpr std::string_view CloseOtherPanesKey{ "closeOtherPanes" };
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
static constexpr std::string_view MovePaneKey{ "movePane" };
static constexpr std::string_view SwapPaneKey{ "swapPane" };
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
static constexpr std::string_view ScrollDownKey{ "scrollDown" };
static constexpr std::string_view ScrollDownPageKey{ "scrollDownPage" };
static constexpr std::string_view ScrollUpKey{ "scrollUp" };
static constexpr std::string_view ScrollUpPageKey{ "scrollUpPage" };
static constexpr std::string_view ScrollToTopKey{ "scrollToTop" };
static constexpr std::string_view ScrollToBottomKey{ "scrollToBottom" };
static constexpr std::string_view ScrollToMarkKey{ "scrollToMark" };
static constexpr std::string_view AddMarkKey{ "addMark" };
static constexpr std::string_view ClearMarkKey{ "clearMark" };
static constexpr std::string_view ClearAllMarksKey{ "clearAllMarks" };
static constexpr std::string_view SendInputKey{ "sendInput" };
static constexpr std::string_view SetColorSchemeKey{ "setColorScheme" };
static constexpr std::string_view SetTabColorKey{ "setTabColor" };
static constexpr std::string_view SplitPaneKey{ "splitPane" };
static constexpr std::string_view SwitchToTabKey{ "switchToTab" };
static constexpr std::string_view TabSearchKey{ "tabSearch" };
static constexpr std::string_view ToggleAlwaysOnTopKey{ "toggleAlwaysOnTop" };
static constexpr std::string_view ToggleCommandPaletteKey{ "commandPalette" };
static constexpr std::string_view SuggestionsKey{ "showSuggestions" };
static constexpr std::string_view ToggleFocusModeKey{ "toggleFocusMode" };
static constexpr std::string_view SetFocusModeKey{ "setFocusMode" };
static constexpr std::string_view ToggleFullscreenKey{ "toggleFullscreen" };
static constexpr std::string_view SetFullScreenKey{ "setFullScreen" };
static constexpr std::string_view SetMaximizedKey{ "setMaximized" };
static constexpr std::string_view TogglePaneZoomKey{ "togglePaneZoom" };
static constexpr std::string_view ToggleSplitOrientationKey{ "toggleSplitOrientation" };
static constexpr std::string_view LegacyToggleRetroEffectKey{ "toggleRetroEffect" };
static constexpr std::string_view ToggleShaderEffectsKey{ "toggleShaderEffects" };
static constexpr std::string_view MoveTabKey{ "moveTab" };
static constexpr std::string_view BreakIntoDebuggerKey{ "breakIntoDebugger" };
static constexpr std::string_view FindMatchKey{ "findMatch" };
static constexpr std::string_view TogglePaneReadOnlyKey{ "toggleReadOnlyMode" };
static constexpr std::string_view EnablePaneReadOnlyKey{ "enableReadOnlyMode" };
static constexpr std::string_view DisablePaneReadOnlyKey{ "disableReadOnlyMode" };
static constexpr std::string_view NewWindowKey{ "newWindow" };
static constexpr std::string_view IdentifyWindowKey{ "identifyWindow" };
static constexpr std::string_view IdentifyWindowsKey{ "identifyWindows" };
static constexpr std::string_view RenameWindowKey{ "renameWindow" };
static constexpr std::string_view OpenWindowRenamerKey{ "openWindowRenamer" };
static constexpr std::string_view DisplayWorkingDirectoryKey{ "debugTerminalCwd" };
static constexpr std::string_view SearchForTextKey{ "searchWeb" };
static constexpr std::string_view GlobalSummonKey{ "globalSummon" };
static constexpr std::string_view QuakeModeKey{ "quakeMode" };
static constexpr std::string_view FocusPaneKey{ "focusPane" };
static constexpr std::string_view OpenSystemMenuKey{ "openSystemMenu" };
static constexpr std::string_view ExportBufferKey{ "exportBuffer" };
static constexpr std::string_view ClearBufferKey{ "clearBuffer" };
static constexpr std::string_view MultipleActionsKey{ "multipleActions" };
static constexpr std::string_view QuitKey{ "quit" };
static constexpr std::string_view AdjustOpacityKey{ "adjustOpacity" };
static constexpr std::string_view RestoreLastClosedKey{ "restoreLastClosed" };
static constexpr std::string_view SelectAllKey{ "selectAll" };
static constexpr std::string_view SelectCommandKey{ "selectCommand" };
static constexpr std::string_view SelectOutputKey{ "selectOutput" };
static constexpr std::string_view MarkModeKey{ "markMode" };
static constexpr std::string_view ToggleBlockSelectionKey{ "toggleBlockSelection" };
static constexpr std::string_view SwitchSelectionEndpointKey{ "switchSelectionEndpoint" };
static constexpr std::string_view ColorSelectionKey{ "experimental.colorSelection" };
static constexpr std::string_view ShowContextMenuKey{ "showContextMenu" };
static constexpr std::string_view ExpandSelectionToWordKey{ "expandSelectionToWord" };
static constexpr std::string_view RestartConnectionKey{ "restartConnection" };
static constexpr std::string_view ToggleBroadcastInputKey{ "toggleBroadcastInput" };
static constexpr std::string_view OpenScratchpadKey{ "experimental.openScratchpad" };
static constexpr std::string_view OpenAboutKey{ "openAbout" };
static constexpr std::string_view QuickFixKey{ "quickFix" };
static constexpr std::string_view OpenCWDKey{ "openCWD" };

static constexpr std::string_view ActionKey{ "action" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    using namespace ::Microsoft::Terminal::Settings::Model;

    using ParseActionFunction = FromJsonResult (*)(const Json::Value&);
    using SerializeActionFunction = Json::Value (*)(const IActionArgs&);

    using KeyToActionPair = std::pair<std::string_view, ShortcutAction>;
    using ActionToKeyPair = std::pair<ShortcutAction, std::string_view>;
    using SerializersPair = std::pair<ParseActionFunction, SerializeActionFunction>;
    using ActionToSerializersPair = std::pair<ShortcutAction, SerializersPair>;

#define KEY_TO_ACTION_PAIR(action) KeyToActionPair{ action##Key, ShortcutAction::action },
#define ACTION_TO_KEY_PAIR(action) ActionToKeyPair{ ShortcutAction::action, action##Key },
#define ACTION_TO_SERIALIZERS_PAIR(action) ActionToSerializersPair{ ShortcutAction::action, { action##Args::FromJson, action##Args::ToJson } },

    static constexpr til::static_map ActionKeyNamesMap{
#define ON_ALL_ACTIONS(action) KEY_TO_ACTION_PAIR(action)
        ALL_SHORTCUT_ACTIONS
    // Don't include the INTERNAL_SHORTCUT_ACTIONS here
#undef ON_ALL_ACTIONS
    };

    static constexpr til::static_map ActionToStringMap{
#define ON_ALL_ACTIONS(action) ACTION_TO_KEY_PAIR(action)
        ALL_SHORTCUT_ACTIONS
    // Don't include the INTERNAL_SHORTCUT_ACTIONS here
#undef ON_ALL_ACTIONS
    };

    // This is a map of ShortcutAction->{function<IActionArgs(Json::Value)>, function<Json::Value(IActionArgs)>. It holds
    // a set of (de)serializer functions that can be used to (de)serialize an IActionArgs
    // from json. Each type of IActionArgs that can accept arbitrary args should be
    // placed into this map, with the corresponding deserializer function as the
    // value.
    static constexpr til::static_map argSerializerMap{

        // These are special cases.
        // - QuakeMode: deserializes into a GlobalSummon, so we don't need a serializer
        // - Invalid: has no args
        ActionToSerializersPair{ ShortcutAction::QuakeMode, { &GlobalSummonArgs::QuakeModeFromJson, nullptr } },
        ActionToSerializersPair{ ShortcutAction::Invalid, { nullptr, nullptr } },

#define ON_ALL_ACTIONS_WITH_ARGS(action) ACTION_TO_SERIALIZERS_PAIR(action)
        ALL_SHORTCUT_ACTIONS_WITH_ARGS
    // Don't include the INTERNAL_SHORTCUT_ACTIONS here
#undef ON_ALL_ACTIONS_WITH_ARGS
    };

    ActionAndArgs::ActionAndArgs(ShortcutAction action)
    {
        // Find the deserializer
        const auto deserializersIter = argSerializerMap.find(action);
        if (deserializersIter != argSerializerMap.end())
        {
            auto pfn = deserializersIter->second.first;
            if (pfn)
            {
                // Call the deserializer on an empty JSON object.
                // This ensures that we have a valid ActionArgs
                std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> parseWarnings;
                std::tie(_Args, parseWarnings) = pfn({});
            }

            // if an arg parser was registered, but failed,
            // return the invalid ActionAndArgs we started with.
            if (pfn && _Args == nullptr)
            {
                return;
            }
        }

        // Either...
        // (1) we don't have a deserializer, so it's ok for _Args to be null, or
        // (2) we had one AND it worked, so _Args is set up properly
        _Action = action;
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
        const auto found = ActionKeyNamesMap.find(actionString);
        return found != ActionKeyNamesMap.end() ? found->second : ShortcutAction::Invalid;
    }

    // Method Description:
    // - Deserialize an ActionAndArgs from the provided json object or string `json`.
    //   * If json is a string, we'll attempt to treat it as an action name,
    //     without arguments.
    //   * If json is an object, we'll attempt to retrieve the action name from
    //     its "action" property, and we'll use that name to find a deserializer
    //     to process the rest of the arguments in the json object.
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
        auto action = ShortcutAction::Invalid;

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
        const auto deserializersIter = argSerializerMap.find(action);
        if (deserializersIter != argSerializerMap.end())
        {
            auto pfn = deserializersIter->second.first;
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

    Json::Value ActionAndArgs::ToJson(const Model::ActionAndArgs& val)
    {
        if (val)
        {
            // Search for the ShortcutAction
            const auto shortcutActionIter{ ActionToStringMap.find(val.Action()) };
            if (shortcutActionIter == ActionToStringMap.end())
            {
                // Couldn't find the ShortcutAction,
                // return... "command": "unbound"
                return static_cast<std::string>(UnboundKey);
            }

            if (!val.Args())
            {
                // No args to serialize,
                // output something like... "command": "copy"
                return static_cast<std::string>(shortcutActionIter->second);
            }
            else
            {
                // Serialize any set args,
                // output something like... "command": { "action": "copy", "singleLine": false }
                Json::Value result{ Json::ValueType::objectValue };

                // Set the action args, if any
                const auto actionArgSerializerIter{ argSerializerMap.find(val.Action()) };
                if (actionArgSerializerIter != argSerializerMap.end())
                {
                    auto pfn{ actionArgSerializerIter->second.second };
                    if (pfn)
                    {
                        result = pfn(val.Args());
                    }
                }

                // Set the "action" part
                result[static_cast<std::string>(ActionKey)] = static_cast<std::string>(shortcutActionIter->second);

                return result;
            }
        }

        // "command": "unbound"
        return static_cast<std::string>(UnboundKey);
    }

    com_ptr<ActionAndArgs> ActionAndArgs::Copy() const
    {
        auto copy{ winrt::make_self<ActionAndArgs>() };
        copy->_Action = _Action;
        copy->_Args = _Args ? _Args.Copy() : IActionArgs{ nullptr };
        return copy;
    }

    winrt::hstring ActionAndArgs::GenerateName(const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext& context) const
    {
        if (_Args)
        {
            auto nameFromArgs = _Args.GenerateName(context);
            if (!nameFromArgs.empty())
            {
                return nameFromArgs;
            }
        }

        return ActionArgFactory::GetNameForAction(_Action, context);
    }

    winrt::hstring ActionAndArgs::GenerateName() const
    {
        return GenerateName(GetLibraryResourceLoader().ResourceContext());
    }

    // Function Description:
    // - This will generate an ID for this ActionAndArgs, based on the ShortcutAction and the Args
    // - It will always create the same ID if the ShortcutAction and the Args are the same
    // - Note: this should only be called for User-created actions
    // - Example: The "SendInput 'abc'" action will have the generated ID "User.sendInput.<hash of 'abc'>"
    // Return Value:
    // - The ID, based on the ShortcutAction and the Args
    winrt::hstring ActionAndArgs::GenerateID() const
    {
        if (_Action != ShortcutAction::Invalid)
        {
            auto actionKeyString = ActionToStringMap.find(_Action)->second;
            auto result = fmt::format(FMT_COMPILE(L"User.{}"), winrt::to_hstring(actionKeyString));
            if (_Args)
            {
                // If there are args, we need to append the hash of the args
                // However, to make it a little more presentable we
                // 1. truncate the hash to 32 bits
                // 2. convert it to a hex string
                // there is a _tiny_ chance of collision because of the truncate but unlikely for
                // the number of commands a user is expected to have
                const auto argsHash32 = static_cast<uint32_t>(_Args.Hash() & 0xFFFFFFFF);
                // {0:X} formats the truncated hash to an uppercase hex string
                fmt::format_to(std::back_inserter(result), FMT_COMPILE(L".{:X}"), argsHash32);
            }
            return winrt::hstring{ result };
        }
        return {};
    }

    winrt::hstring ActionAndArgs::Serialize(const winrt::Windows::Foundation::Collections::IVector<Model::ActionAndArgs>& args)
    {
        Json::Value json{ Json::objectValue };
        JsonUtils::SetValueForKey(json, "actions", args);
        Json::StreamWriterBuilder wbuilder;
        auto str = Json::writeString(wbuilder, json);
        return winrt::to_hstring(str);
    }
    winrt::Windows::Foundation::Collections::IVector<Model::ActionAndArgs> ActionAndArgs::Deserialize(winrt::hstring content)
    {
        auto data = winrt::to_string(content);

        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
        Json::Value root;
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
        }

        winrt::Windows::Foundation::Collections::IVector<Model::ActionAndArgs> result{ nullptr };
        JsonUtils::GetValueForKey(root, "actions", result);
        return result;
    }

}
