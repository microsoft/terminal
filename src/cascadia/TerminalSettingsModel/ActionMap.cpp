// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AllShortcutActions.h"
#include "ActionMap.h"
#include "Command.h"
#include <LibraryResources.h>
#include <til/io.h>

#include "ActionMap.g.cpp"
#include "ActionArgFactory.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    static InternalActionID Hash(const Model::ActionAndArgs& actionAndArgs)
    {
        til::hasher hasher;

        // action will be hashed last.
        // This allows us to first seed a til::hasher
        // with the return value of IActionArgs::Hash().
        const auto action = actionAndArgs.Action();

        if (const auto args = actionAndArgs.Args())
        {
            hasher = til::hasher{ gsl::narrow_cast<size_t>(args.Hash()) };
        }
        else
        {
            size_t hash = 0;

            // Args are not defined.
            // Check if the ShortcutAction supports args.
            switch (action)
            {
#define ON_ALL_ACTIONS_WITH_ARGS(action)                               \
    case ShortcutAction::action:                                       \
    {                                                                  \
        /* If it does, hash the default values for the args. */        \
        static const auto cachedHash = gsl::narrow_cast<size_t>(       \
            winrt::make_self<implementation::action##Args>()->Hash()); \
        hash = cachedHash;                                             \
        break;                                                         \
    }
                ALL_SHORTCUT_ACTIONS_WITH_ARGS
                INTERNAL_SHORTCUT_ACTIONS_WITH_ARGS
#undef ON_ALL_ACTIONS_WITH_ARGS
            default:
                break;
            }

            hasher = til::hasher{ hash };
        }

        hasher.write(action);
        return hasher.finalize();
    }

    winrt::hstring ActionArgFactory::GetNameForAction(Model::ShortcutAction action)
    {
        return GetNameForAction(action, GetLibraryResourceLoader().ResourceContext());
    }

    winrt::hstring ActionArgFactory::GetNameForAction(Model::ShortcutAction action, Windows::ApplicationModel::Resources::Core::ResourceContext context)
    {
        // Use a magic static to initialize this map, because we won't be able
        // to load the resources at _init_, only at runtime.
        static auto actionNames = []() {
            return std::unordered_map<ShortcutAction, wil::zwstring_view>{
                { ShortcutAction::AddMark, USES_RESOURCE(L"AddMarkCommandKey") },
                { ShortcutAction::AdjustFontSize, USES_RESOURCE(L"AdjustFontSizeCommandKey") },
                { ShortcutAction::AdjustOpacity, USES_RESOURCE(L"AdjustOpacity") },
                { ShortcutAction::BreakIntoDebugger, USES_RESOURCE(L"BreakIntoDebuggerCommandKey") },
                { ShortcutAction::ClearAllMarks, USES_RESOURCE(L"ClearAllMarksCommandKey") },
                { ShortcutAction::ClearBuffer, USES_RESOURCE(L"ClearBuffer") },
                { ShortcutAction::ClearMark, USES_RESOURCE(L"ClearMarkCommandKey") },
                { ShortcutAction::CloseOtherPanes, USES_RESOURCE(L"CloseOtherPanesCommandKey") },
                { ShortcutAction::CloseOtherTabs, USES_RESOURCE(L"CloseOtherTabs") },
                { ShortcutAction::ClosePane, USES_RESOURCE(L"ClosePaneCommandKey") },
                { ShortcutAction::CloseTab, USES_RESOURCE(L"CloseTab") },
                { ShortcutAction::CloseTabsAfter, USES_RESOURCE(L"CloseTabsAfter") },
                { ShortcutAction::CloseWindow, USES_RESOURCE(L"CloseWindowCommandKey") },
                { ShortcutAction::ColorSelection, USES_RESOURCE(L"ColorSelection") },
                { ShortcutAction::CopyText, USES_RESOURCE(L"CopyTextCommandKey") },
                { ShortcutAction::DisplayWorkingDirectory, USES_RESOURCE(L"DisplayWorkingDirectoryCommandKey") },
                { ShortcutAction::DisablePaneReadOnly, USES_RESOURCE(L"DisablePaneReadOnlyCommandKey") },
                { ShortcutAction::DuplicateTab, USES_RESOURCE(L"DuplicateTabCommandKey") },
                { ShortcutAction::EnablePaneReadOnly, USES_RESOURCE(L"EnablePaneReadOnlyCommandKey") },
                { ShortcutAction::ExecuteCommandline, USES_RESOURCE(L"ExecuteCommandlineCommandKey") },
                { ShortcutAction::ExportBuffer, USES_RESOURCE(L"ExportBuffer") },
                { ShortcutAction::ExpandSelectionToWord, USES_RESOURCE(L"ExpandSelectionToWordCommandKey") },
                { ShortcutAction::Find, USES_RESOURCE(L"FindCommandKey") },
                { ShortcutAction::FindMatch, USES_RESOURCE(L"FindMatch") },
                { ShortcutAction::FocusPane, USES_RESOURCE(L"FocusPane") },
                { ShortcutAction::GlobalSummon, USES_RESOURCE(L"GlobalSummonCommandKey") },
                { ShortcutAction::IdentifyWindow, USES_RESOURCE(L"IdentifyWindowCommandKey") },
                { ShortcutAction::IdentifyWindows, USES_RESOURCE(L"IdentifyWindowsCommandKey") },
                { ShortcutAction::MarkMode, USES_RESOURCE(L"MarkModeCommandKey") },
                { ShortcutAction::MoveFocus, USES_RESOURCE(L"MoveFocusCommandKey") },
                { ShortcutAction::MovePane, USES_RESOURCE(L"MovePaneCommandKey") },
                { ShortcutAction::MoveTab, USES_RESOURCE(L"MoveTab") },
                { ShortcutAction::MultipleActions, USES_RESOURCE(L"MultipleActions") },
                { ShortcutAction::NewTab, USES_RESOURCE(L"NewTabCommandKey") },
                { ShortcutAction::NewWindow, USES_RESOURCE(L"NewWindowCommandKey") },
                { ShortcutAction::NextTab, USES_RESOURCE(L"NextTabCommandKey") },
                { ShortcutAction::OpenAbout, USES_RESOURCE(L"OpenAboutCommandKey") },
                { ShortcutAction::OpenCWD, USES_RESOURCE(L"OpenCWDCommandKey") },
                { ShortcutAction::OpenNewTabDropdown, USES_RESOURCE(L"OpenNewTabDropdownCommandKey") },
                { ShortcutAction::OpenScratchpad, USES_RESOURCE(L"OpenScratchpadKey") },
                { ShortcutAction::OpenSettings, USES_RESOURCE(L"OpenSettingsUICommandKey") },
                { ShortcutAction::OpenSystemMenu, USES_RESOURCE(L"OpenSystemMenuCommandKey") },
                { ShortcutAction::OpenTabColorPicker, USES_RESOURCE(L"OpenTabColorPickerCommandKey") },
                { ShortcutAction::OpenTabRenamer, USES_RESOURCE(L"OpenTabRenamerCommandKey") },
                { ShortcutAction::OpenWindowRenamer, USES_RESOURCE(L"OpenWindowRenamerCommandKey") },
                { ShortcutAction::PasteText, USES_RESOURCE(L"PasteTextCommandKey") },
                { ShortcutAction::PrevTab, USES_RESOURCE(L"PrevTabCommandKey") },
                { ShortcutAction::QuickFix, USES_RESOURCE(L"QuickFixCommandKey") },
                { ShortcutAction::QuakeMode, USES_RESOURCE(L"QuakeModeCommandKey") },
                { ShortcutAction::Quit, USES_RESOURCE(L"QuitCommandKey") },
                { ShortcutAction::RenameTab, USES_RESOURCE(L"ResetTabNameCommandKey") },
                { ShortcutAction::RenameWindow, USES_RESOURCE(L"ResetWindowNameCommandKey") },
                { ShortcutAction::ResetFontSize, USES_RESOURCE(L"ResetFontSizeCommandKey") },
                { ShortcutAction::RestartConnection, USES_RESOURCE(L"RestartConnectionKey") },
                { ShortcutAction::ResizePane, USES_RESOURCE(L"ResizePaneCommandKey") },
                { ShortcutAction::RestoreLastClosed, USES_RESOURCE(L"RestoreLastClosedCommandKey") },
                { ShortcutAction::SaveSnippet, USES_RESOURCE(L"SaveSnippetNamePrefix") },
                { ShortcutAction::ScrollDown, USES_RESOURCE(L"ScrollDownCommandKey") },
                { ShortcutAction::ScrollDownPage, USES_RESOURCE(L"ScrollDownPageCommandKey") },
                { ShortcutAction::ScrollToBottom, USES_RESOURCE(L"ScrollToBottomCommandKey") },
                { ShortcutAction::ScrollToMark, USES_RESOURCE(L"ScrollToPreviousMarkCommandKey") },
                { ShortcutAction::ScrollToTop, USES_RESOURCE(L"ScrollToTopCommandKey") },
                { ShortcutAction::ScrollUp, USES_RESOURCE(L"ScrollUpCommandKey") },
                { ShortcutAction::ScrollUpPage, USES_RESOURCE(L"ScrollUpPageCommandKey") },
                { ShortcutAction::SearchForText, USES_RESOURCE(L"SearchForText") },
                { ShortcutAction::SelectAll, USES_RESOURCE(L"SelectAllCommandKey") },
                { ShortcutAction::SelectCommand, USES_RESOURCE(L"SelectCommand") },
                { ShortcutAction::SelectOutput, USES_RESOURCE(L"SelectOutput") },
                { ShortcutAction::SendInput, USES_RESOURCE(L"SendInput") },
                { ShortcutAction::SetColorScheme, USES_RESOURCE(L"SetColorScheme") },
                { ShortcutAction::SetFocusMode, USES_RESOURCE(L"SetFocusMode") },
                { ShortcutAction::SetFullScreen, USES_RESOURCE(L"SetFullScreen") },
                { ShortcutAction::SetMaximized, USES_RESOURCE(L"SetMaximized") },
                { ShortcutAction::SetTabColor, USES_RESOURCE(L"ResetTabColorCommandKey") },
                { ShortcutAction::ShowContextMenu, USES_RESOURCE(L"ShowContextMenuCommandKey") },
                { ShortcutAction::SplitPane, USES_RESOURCE(L"SplitPaneCommandKey") },
                { ShortcutAction::Suggestions, USES_RESOURCE(L"Suggestions") },
                { ShortcutAction::SwapPane, USES_RESOURCE(L"SwapPaneCommandKey") },
                { ShortcutAction::SwitchSelectionEndpoint, USES_RESOURCE(L"SwitchSelectionEndpointCommandKey") },
                { ShortcutAction::SwitchToTab, USES_RESOURCE(L"SwitchToTabCommandKey") },
                { ShortcutAction::TabSearch, USES_RESOURCE(L"TabSearchCommandKey") },
                { ShortcutAction::ToggleAlwaysOnTop, USES_RESOURCE(L"ToggleAlwaysOnTopCommandKey") },
                { ShortcutAction::ToggleBlockSelection, USES_RESOURCE(L"ToggleBlockSelectionCommandKey") },
                { ShortcutAction::ToggleBroadcastInput, USES_RESOURCE(L"ToggleBroadcastInputCommandKey") },
                { ShortcutAction::ToggleCommandPalette, USES_RESOURCE(L"ToggleCommandPaletteCommandKey") },
                { ShortcutAction::ToggleFocusMode, USES_RESOURCE(L"ToggleFocusModeCommandKey") },
                { ShortcutAction::ToggleFullscreen, USES_RESOURCE(L"ToggleFullscreenCommandKey") },
                { ShortcutAction::TogglePaneReadOnly, USES_RESOURCE(L"TogglePaneReadOnlyCommandKey") },
                { ShortcutAction::TogglePaneZoom, USES_RESOURCE(L"TogglePaneZoomCommandKey") },
                { ShortcutAction::ToggleShaderEffects, USES_RESOURCE(L"ToggleShaderEffectsCommandKey") },
                { ShortcutAction::ToggleSplitOrientation, USES_RESOURCE(L"ToggleSplitOrientationCommandKey") },
            };
        }();

        const auto found = actionNames.find(action);
        if (found != actionNames.end() && !found->second.empty())
        {
            return GetLibraryResourceLoader().ResourceMap().GetValue(found->second, context).ValueAsString();
        }
        return winrt::hstring{};
    }

    winrt::Windows::Foundation::Collections::IMap<Model::ShortcutAction, winrt::hstring> ActionArgFactory::AvailableShortcutActionsAndNames()
    {
        std::unordered_map<ShortcutAction, winrt::hstring> actionNames;
#define ON_ALL_ACTIONS(action) actionNames.emplace(ShortcutAction::action, GetNameForAction(ShortcutAction::action));

        ALL_SHORTCUT_ACTIONS

#undef ON_ALL_ACTIONS
        return winrt::single_threaded_map(std::move(actionNames));
    }

    Model::IActionArgs ActionArgFactory::GetEmptyArgsForAction(Model::ShortcutAction shortcutAction)
    {
        // TODO: GH 19056 - we cannot cleanly macro this because of some special cases (SplitPaneArgs, NewTabArgs, NewWindowArgs)
        // where we initialize a NewTerminalArgs object to pass in to them because the settings UI cannot currently handle
        // a ContentArgs object that is not a NewTerminalArgs
        switch (shortcutAction)
        {
        case Model::ShortcutAction::AdjustFontSize:
            return winrt::make<AdjustFontSizeArgs>();
        case Model::ShortcutAction::CloseOtherTabs:
            return winrt::make<CloseOtherTabsArgs>();
        case Model::ShortcutAction::CloseTabsAfter:
            return winrt::make<CloseTabsAfterArgs>();
        case Model::ShortcutAction::CloseTab:
            return winrt::make<CloseTabArgs>();
        case Model::ShortcutAction::CopyText:
            return winrt::make<CopyTextArgs>();
        case Model::ShortcutAction::ExecuteCommandline:
            return winrt::make<ExecuteCommandlineArgs>();
        case Model::ShortcutAction::FindMatch:
            return winrt::make<FindMatchArgs>();
        case Model::ShortcutAction::SearchForText:
            return winrt::make<SearchForTextArgs>();
        case Model::ShortcutAction::GlobalSummon:
            return winrt::make<GlobalSummonArgs>();
        case Model::ShortcutAction::MoveFocus:
            return winrt::make<MoveFocusArgs>();
        case Model::ShortcutAction::MovePane:
            return winrt::make<MovePaneArgs>();
        case Model::ShortcutAction::SwapPane:
            return winrt::make<SwapPaneArgs>();
        case Model::ShortcutAction::MoveTab:
            return winrt::make<MoveTabArgs>();
        case Model::ShortcutAction::NewTab:
            return winrt::make<NewTabArgs>(Model::NewTerminalArgs{});
        case Model::ShortcutAction::NewWindow:
            return winrt::make<NewWindowArgs>(Model::NewTerminalArgs{});
        case Model::ShortcutAction::NextTab:
            return winrt::make<NextTabArgs>();
        case Model::ShortcutAction::OpenSettings:
            return winrt::make<OpenSettingsArgs>();
        case Model::ShortcutAction::SetFocusMode:
            return winrt::make<SetFocusModeArgs>();
        case Model::ShortcutAction::SetFullScreen:
            return winrt::make<SetFullScreenArgs>();
        case Model::ShortcutAction::SetMaximized:
            return winrt::make<SetMaximizedArgs>();
        case Model::ShortcutAction::PrevTab:
            return winrt::make<PrevTabArgs>();
        case Model::ShortcutAction::RenameTab:
            return winrt::make<RenameTabArgs>();
        case Model::ShortcutAction::RenameWindow:
            return winrt::make<RenameWindowArgs>();
        case Model::ShortcutAction::ResizePane:
            return winrt::make<ResizePaneArgs>();
        case Model::ShortcutAction::ScrollDown:
            return winrt::make<ScrollDownArgs>();
        case Model::ShortcutAction::ScrollUp:
            return winrt::make<ScrollUpArgs>();
        case Model::ShortcutAction::ScrollToMark:
            return winrt::make<ScrollToMarkArgs>();
        case Model::ShortcutAction::AddMark:
            return winrt::make<AddMarkArgs>();
        case Model::ShortcutAction::SendInput:
            return winrt::make<SendInputArgs>();
        case Model::ShortcutAction::SetColorScheme:
            return winrt::make<SetColorSchemeArgs>();
        case Model::ShortcutAction::SetTabColor:
            return winrt::make<SetTabColorArgs>();
        case Model::ShortcutAction::SplitPane:
            return winrt::make<SplitPaneArgs>(SplitDirection::Automatic, Model::NewTerminalArgs{});
        case Model::ShortcutAction::SwitchToTab:
            return winrt::make<SwitchToTabArgs>();
        case Model::ShortcutAction::ToggleCommandPalette:
            return winrt::make<ToggleCommandPaletteArgs>();
        case Model::ShortcutAction::FocusPane:
            return winrt::make<FocusPaneArgs>();
        case Model::ShortcutAction::ExportBuffer:
            return winrt::make<ExportBufferArgs>();
        case Model::ShortcutAction::ClearBuffer:
            return winrt::make<ClearBufferArgs>();
        case Model::ShortcutAction::MultipleActions:
            return winrt::make<MultipleActionsArgs>();
        case Model::ShortcutAction::AdjustOpacity:
            return winrt::make<AdjustOpacityArgs>();
        case Model::ShortcutAction::Suggestions:
            return winrt::make<SuggestionsArgs>();
        case Model::ShortcutAction::SelectCommand:
            return winrt::make<SelectCommandArgs>();
        case Model::ShortcutAction::SelectOutput:
            return winrt::make<SelectOutputArgs>();
        case Model::ShortcutAction::ColorSelection:
            return winrt::make<ColorSelectionArgs>();
        default:
            return nullptr;
        }
    }

    // Method Description:
    // - Detects if any of the user's actions are identical to the inbox actions,
    //   and if so, deletes them and redirects their keybindings to the inbox actions
    // - We have to do this here instead of when loading since we don't actually have
    //   any parents while loading the user settings, the parents are added after
    void ActionMap::_FinalizeInheritance()
    {
        // first, gather the inbox actions from the relevant parent
        std::unordered_map<InternalActionID, Model::Command> inboxActions;
        winrt::com_ptr<implementation::ActionMap> foundParent{ nullptr };
        for (const auto& parent : _parents)
        {
            const auto parentMap = parent->_ActionMap;
            if (parentMap.begin() != parentMap.end() && parentMap.begin()->second.Origin() == OriginTag::InBox)
            {
                // only one parent contains all the inbox actions and that parent contains only inbox actions,
                // so if we found an inbox action we know this is the parent we are looking for
                foundParent = parent;
                break;
            }
        }

        if (foundParent)
        {
            for (const auto& [_, cmd] : foundParent->_ActionMap)
            {
                inboxActions.emplace(Hash(cmd.ActionAndArgs()), cmd);
            }
        }

        std::unordered_map<KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality> keysToReassign;

        // now, look through our _ActionMap for commands that
        // - had an ID generated for them
        // - do not have a name/icon path
        // - have a hash that matches a command in the inbox actions
        std::erase_if(_ActionMap, [&](const auto& pair) {
            const auto userCmdImpl{ get_self<Command>(pair.second) };
            if (userCmdImpl->IDWasGenerated() && !userCmdImpl->HasName() && userCmdImpl->Icon().Path().empty())
            {
                const auto userActionHash = Hash(userCmdImpl->ActionAndArgs());
                if (const auto inboxCmd = inboxActions.find(userActionHash); inboxCmd != inboxActions.end())
                {
                    for (const auto& [key, cmdID] : _KeyMap)
                    {
                        // for any of our keys that point to the user action, point them to the inbox action instead
                        if (cmdID == pair.first)
                        {
                            keysToReassign.insert_or_assign(key, inboxCmd->second.ID());
                        }
                    }

                    // remove this pair
                    return true;
                }
            }
            return false;
        });

        for (const auto [key, cmdID] : keysToReassign)
        {
            _KeyMap.insert_or_assign(key, cmdID);
        }
    }

    bool ActionMap::FixupsAppliedDuringLoad() const
    {
        return _fixupsAppliedDuringLoad;
    }

    // Method Description:
    // - Retrieves the Command referred to be the given ID
    // - Will recurse through parents if we don't find it in this layer
    // Arguments:
    // - actionID: the internal ID associated with a Command
    // Return Value:
    // - The command if it exists in this layer; otherwise, nullptr
    Model::Command ActionMap::_GetActionByID(const winrt::hstring& actionID) const
    {
        // Check current layer
        const auto actionMapPair{ _ActionMap.find(actionID) };
        if (actionMapPair != _ActionMap.end())
        {
            auto& cmd{ actionMapPair->second };

            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(cmd);

            return cmd;
        }

        for (const auto& parent : _parents)
        {
            if (const auto inheritedCmd = parent->_GetActionByID(actionID))
            {
                return inheritedCmd;
            }
        }

        // We don't have an answer
        return nullptr;
    }

    static void RegisterShortcutAction(ShortcutAction shortcutAction, std::unordered_map<hstring, Model::ActionAndArgs>& list, std::unordered_set<InternalActionID>& visited)
    {
        const auto actionAndArgs{ make_self<ActionAndArgs>(shortcutAction) };
        /*We have a valid action.*/
        /*Check if the action was already added.*/
        if (visited.find(Hash(*actionAndArgs)) == visited.end())
        {
            /*This is an action that wasn't added!*/
            /*Let's add it if it has a name.*/
            if (const auto name{ actionAndArgs->GenerateName() }; !name.empty())
            {
                list.insert({ name, *actionAndArgs });
            }
        }
    }

    // Method Description:
    // - Retrieves a map of actions that can be bound to a key
    IMapView<hstring, Model::ActionAndArgs> ActionMap::AvailableActions()
    {
        if (!_AvailableActionsCache)
        {
            // populate _AvailableActionsCache
            std::unordered_map<hstring, Model::ActionAndArgs> availableActions;
            std::unordered_set<InternalActionID> visitedActionIDs;
            _PopulateAvailableActionsWithStandardCommands(availableActions, visitedActionIDs);

// now add any ShortcutActions that we might have missed
#define ON_ALL_ACTIONS(action) RegisterShortcutAction(ShortcutAction::action, availableActions, visitedActionIDs);
            ALL_SHORTCUT_ACTIONS
            // Don't include internal actions here
#undef ON_ALL_ACTIONS

            _AvailableActionsCache = single_threaded_map(std::move(availableActions));
        }
        return _AvailableActionsCache.GetView();
    }

    void ActionMap::_PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const
    {
        // Update AvailableActions and visitedActionIDs with our current layer
        for (const auto& [_, cmd] : _ActionMap)
        {
            // Only populate AvailableActions with actions that haven't been visited already.
            const auto actionID = Hash(cmd.ActionAndArgs());
            if (!visitedActionIDs.contains(actionID))
            {
                const auto name{ cmd.Name() };
                if (!name.empty())
                {
                    // Update AvailableActions.
                    const auto actionAndArgsImpl{ get_self<ActionAndArgs>(cmd.ActionAndArgs()) };
                    availableActions.insert_or_assign(name, *actionAndArgsImpl->Copy());
                }

                // Record that we already handled adding this action to the NameMap.
                visitedActionIDs.insert(actionID);
            }
        }

        // Update NameMap and visitedActionIDs with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateAvailableActionsWithStandardCommands(availableActions, visitedActionIDs);
        }
    }

    // Method Description:
    // - Retrieves a map of command names to the commands themselves
    // - These commands should not be modified directly because they may result in
    //    an invalid state for the `ActionMap`
    IMapView<hstring, Model::Command> ActionMap::NameMap()
    {
        if (!_NameMapCache)
        {
            if (_CumulativeIDToActionMapCache.empty())
            {
                _RefreshKeyBindingCaches();
            }
            // populate _NameMapCache
            std::unordered_map<hstring, Model::Command> nameMap{};
            _PopulateNameMapWithSpecialCommands(nameMap);
            _PopulateNameMapWithStandardCommands(nameMap);

            _NameMapCache = single_threaded_map(std::move(nameMap));
        }
        return _NameMapCache.GetView();
    }

    // Method Description:
    // - Populates the provided nameMap with all of our special commands and our parent's special commands.
    // - Special commands include nested and iterable commands.
    // - Performs a top-down approach by going to the root first, then recursively adding the nested commands layer-by-layer.
    // Arguments:
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    void ActionMap::_PopulateNameMapWithSpecialCommands(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        // Update NameMap with our parents.
        // Starting with this means we're doing a top-down approach.
        for (const auto& parent : _parents)
        {
            parent->_PopulateNameMapWithSpecialCommands(nameMap);
        }

        // Add NestedCommands to NameMap _after_ we handle our parents.
        // This allows us to override whatever our parents tell us.
        for (const auto& [name, cmd] : _NestedCommands)
        {
            if (cmd.HasNestedCommands())
            {
                // add a valid cmd
                nameMap.insert_or_assign(name, cmd);
            }
            else
            {
                // remove the invalid cmd
                nameMap.erase(name);
            }
        }

        // Add IterableCommands to NameMap
        for (const auto& cmd : _IterableCommands)
        {
            nameMap.insert_or_assign(cmd.Name(), cmd);
        }
    }

    // Method Description:
    // - Populates the provided nameMap with all of our actions and our parents actions
    //    while omitting the actions that were already added before
    // Arguments:
    // - nameMap: the nameMap we're populating, this maps the name (hstring) of a command to the command itself
    void ActionMap::_PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        for (const auto& [_, cmd] : _CumulativeIDToActionMapCache)
        {
            const auto& name{ cmd.Name() };
            if (!name.empty())
            {
                // there might be a collision here, where there could be 2 different commands with the same name
                // in this case, prioritize the user's action
                // TODO GH #17166: we should no longer use Command.Name to identify commands anywhere
                if (!nameMap.contains(name) || cmd.Origin() == OriginTag::User)
                {
                    // either a command with this name does not exist, or this is a user-defined command with a name
                    // in either case, update the name map with the command (if this is a user-defined command with
                    // the same name as an existing command, the existing one will get overwritten)
                    nameMap.insert_or_assign(name, cmd);
                }
            }
        }
    }

    // Method Description:
    // - Recursively populate keyToActionMap with ours and our parents' key -> id pairs
    // - Recursively populate actionToKeyMap with ours and our parents' id -> key pairs
    // - This is a bottom-up approach
    // - Child's pairs override parents' pairs
    void ActionMap::_PopulateCumulativeKeyMaps(std::unordered_map<Control::KeyChord, winrt::hstring, KeyChordHash, KeyChordEquality>& keyToActionMap, std::unordered_map<winrt::hstring, Control::KeyChord>& actionToKeyMap)
    {
        for (const auto& [keys, cmdID] : _KeyMap)
        {
            if (!keyToActionMap.contains(keys))
            {
                keyToActionMap.emplace(keys, cmdID);
            }
            if (!actionToKeyMap.contains(cmdID))
            {
                actionToKeyMap.emplace(cmdID, keys);
            }
        }

        for (const auto& parent : _parents)
        {
            parent->_PopulateCumulativeKeyMaps(keyToActionMap, actionToKeyMap);
        }
    }

    // Method Description:
    // - Recursively populate actionMap with ours and our parents' id -> command pairs
    // - This is a bottom-up approach
    // - Actions of the parents are overridden by the children
    void ActionMap::_PopulateCumulativeActionMap(std::unordered_map<hstring, Model::Command>& actionMap)
    {
        for (const auto& [cmdID, cmd] : _ActionMap)
        {
            if (!actionMap.contains(cmdID))
            {
                actionMap.emplace(cmdID, cmd);
            }
        }

        for (const auto& parent : _parents)
        {
            parent->_PopulateCumulativeActionMap(actionMap);
        }
    }

    IMapView<Control::KeyChord, Model::Command> ActionMap::GlobalHotkeys()
    {
        if (!_GlobalHotkeysCache)
        {
            _RefreshKeyBindingCaches();
        }
        return _GlobalHotkeysCache.GetView();
    }

    IMapView<Control::KeyChord, Model::Command> ActionMap::KeyBindings()
    {
        if (!_ResolvedKeyToActionMapCache)
        {
            _RefreshKeyBindingCaches();
        }
        return _ResolvedKeyToActionMapCache.GetView();
    }

    IVectorView<Model::Command> ActionMap::AllCommands()
    {
        if (!_ResolvedKeyToActionMapCache)
        {
            _RefreshKeyBindingCaches();
        }
        return _AllCommandsCache.GetView();
    }

    void ActionMap::_RefreshKeyBindingCaches()
    {
        _CumulativeKeyToActionMapCache.clear();
        _CumulativeIDToActionMapCache.clear();
        _CumulativeActionToKeyMapCache.clear();
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> globalHotkeys;
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> resolvedKeyToActionMap;
        std::vector<Model::Command> allCommandsVector;

        _PopulateCumulativeKeyMaps(_CumulativeKeyToActionMapCache, _CumulativeActionToKeyMapCache);
        _PopulateCumulativeActionMap(_CumulativeIDToActionMapCache);

        for (const auto& [keys, cmdID] : _CumulativeKeyToActionMapCache)
        {
            if (const auto idCmdPair = _CumulativeIDToActionMapCache.find(cmdID); idCmdPair != _CumulativeIDToActionMapCache.end())
            {
                resolvedKeyToActionMap.emplace(keys, idCmdPair->second);

                // Only populate GlobalHotkeys with actions whose
                // ShortcutAction is GlobalSummon or QuakeMode
                if (idCmdPair->second.ActionAndArgs().Action() == ShortcutAction::GlobalSummon || idCmdPair->second.ActionAndArgs().Action() == ShortcutAction::QuakeMode)
                {
                    globalHotkeys.emplace(keys, idCmdPair->second);
                }
            }
        }

        for (const auto& [_, cmd] : _CumulativeIDToActionMapCache)
        {
            allCommandsVector.emplace_back(cmd);
        }

        _ResolvedKeyToActionMapCache = single_threaded_map(std::move(resolvedKeyToActionMap));
        _GlobalHotkeysCache = single_threaded_map(std::move(globalHotkeys));
        _AllCommandsCache = single_threaded_vector(std::move(allCommandsVector));
    }

    com_ptr<ActionMap> ActionMap::Copy() const
    {
        auto actionMap{ make_self<ActionMap>() };

        // KeyChord --> ID
        actionMap->_KeyMap = _KeyMap;

        // ID --> Command
        actionMap->_ActionMap.reserve(_ActionMap.size());
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            const auto copiedCmd = winrt::get_self<Command>(cmd)->Copy();
            actionMap->_ActionMap.emplace(actionID, *copiedCmd);
        }

        // Name --> Command
        actionMap->_NestedCommands.reserve(_NestedCommands.size());
        for (const auto& [name, cmd] : _NestedCommands)
        {
            actionMap->_NestedCommands.emplace(name, *winrt::get_self<Command>(cmd)->Copy());
        }

        actionMap->_IterableCommands.reserve(_IterableCommands.size());
        for (const auto& cmd : _IterableCommands)
        {
            actionMap->_IterableCommands.emplace_back(*winrt::get_self<Command>(cmd)->Copy());
        }

        actionMap->_parents.reserve(_parents.size());
        for (const auto& parent : _parents)
        {
            actionMap->_parents.emplace_back(parent->Copy());
        }

        return actionMap;
    }

    // Method Description:
    // - Adds a command to the ActionMap
    // Arguments:
    // - cmd: the command we're adding
    void ActionMap::AddAction(const Model::Command& cmd, const Control::KeyChord& keys)
    {
        // _Never_ add null to the ActionMap
        if (!cmd)
        {
            return;
        }

        // invalidate caches
        _CumulativeKeyToActionMapCache.clear();
        _CumulativeIDToActionMapCache.clear();
        _CumulativeActionToKeyMapCache.clear();
        _NameMapCache = nullptr;
        _GlobalHotkeysCache = nullptr;
        _ResolvedKeyToActionMapCache = nullptr;

        // Handle nested commands
        const auto cmdImpl{ get_self<Command>(cmd) };
        if (cmdImpl->IsNestedCommand())
        {
            // But check if it actually has a name to bind to first
            const auto name{ cmd.Name() };
            if (!name.empty())
            {
                _NestedCommands.emplace(name, cmd);
            }
            return;
        }

        // Handle iterable commands
        if (cmdImpl->IterateOn() != ExpandCommandType::None)
        {
            _IterableCommands.emplace_back(cmd);
            return;
        }

        // General Case:
        //  Add the new command to the _ActionMap
        //  Add the new keybinding to the _KeyMap

        _TryUpdateActionMap(cmd);
        _TryUpdateKeyChord(cmd, keys);
    }

    // Method Description:
    // - Try to add the new command to _ActionMap
    // Arguments:
    // - cmd: the action we're trying to register
    void ActionMap::_TryUpdateActionMap(const Model::Command& cmd)
    {
        // if the shortcut action is invalid, then this is for unbinding and _TryUpdateKeyChord will handle that
        if (cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
        {
            const auto cmdImpl{ get_self<implementation::Command>(cmd) };
            if (cmd.Origin() == OriginTag::User && cmd.ID().empty())
            {
                // the user did not define an ID for their non-nested, non-iterable, valid command - generate one for them
                cmdImpl->GenerateID();
            }

            // only add to the _ActionMap if there is an ID
            if (auto cmdID = cmd.ID(); !cmdID.empty())
            {
                // in the legacy scenario, a user might have several of the same action but only one of them has defined an icon or a name
                // eg. { "command": "paste", "name": "myPaste", "keys":"ctrl+a" }
                //     { "command": "paste", "keys": "ctrl+b" }
                // once they port over to the new implementation, we will reduce it to just one Command object with a generated ID
                // but several key binding entries, like so
                //     { "command": "newTab", "id": "User.paste" } -> in the actions map
                //     { "keys": "ctrl+a", "id": "User.paste" }    -> in the keybindings map
                //     { "keys": "ctrl+b", "id": "User.paste" }    -> in the keybindings map
                // however, we have to make sure that we preserve the icon/name that might have been there in one of the command objects
                // to do that, we check if this command we're adding had an ID that was generated
                // if so, we check if there already exists a command with that generated ID, and if there is we port over any name/icon there might be
                // (this may cause us to overwrite in scenarios where the user has an existing command that has the same generated ID but
                //  performs a different action or has different args, but that falls under "play stupid games")
                if (cmdImpl->IDWasGenerated())
                {
                    if (const auto foundCmd{ _GetActionByID(cmdID) })
                    {
                        const auto foundCmdImpl{ get_self<implementation::Command>(foundCmd) };
                        if (foundCmdImpl->HasName() && !cmdImpl->HasName())
                        {
                            cmdImpl->Name(foundCmdImpl->Name());
                        }
                        if (!foundCmdImpl->Icon().Path().empty() && cmdImpl->Icon().Path().empty())
                        {
                            cmdImpl->Icon(foundCmdImpl->Icon());
                        }
                    }
                }
                _ActionMap.insert_or_assign(cmdID, cmd);
            }
        }
    }

    // Method Description:
    // - Update our internal state with the key chord of the newly registered action
    // Arguments:
    // - cmd: the action we're trying to register
    void ActionMap::_TryUpdateKeyChord(const Model::Command& cmd, const Control::KeyChord& keys)
    {
        // Example (this is a legacy case, where the keys are provided in the same block as the command):
        //   {                "command": "copy", "keys": "ctrl+c" } --> we are registering a new key chord
        //   { "name": "foo", "command": "copy" }                   --> no change to keys, exit early
        if (!keys)
        {
            // the user is not trying to update the keys.
            return;
        }

        // Assign the new action in the _KeyMap
        // However, there's a strange edge case here - since we're parsing a legacy or modern block,
        // the user might have { "command": null, "id": "someID", "keys": "ctrl+c" }
        // i.e. they provided an ID for a null command (which they really shouldn't, there's no purpose)
        // in this case, we do _not_ want to use the id they provided, we want to use an empty id
        // (empty id in the _KeyMap indicates the keychord was explicitly unbound)
        const auto action = cmd.ActionAndArgs().Action();
        const auto id = action == ShortcutAction::Invalid ? hstring{} : cmd.ID();
        _KeyMap.insert_or_assign(keys, id);
        _changeLog.emplace(KeysKey);
    }

    // Method Description:
    // - Determines whether the given key chord is explicitly unbound
    // Arguments:
    // - keys: the key chord to check
    // Return value:
    // - true if the keychord is explicitly unbound
    // - false if either the keychord is bound, or not bound at all
    bool ActionMap::IsKeyChordExplicitlyUnbound(const Control::KeyChord& keys) const
    {
        // We use the fact that the ..Internal call returns nullptr for explicitly unbound
        // key chords, and nullopt for keychord that are not bound - it allows us to distinguish
        // between unbound and lack of binding.
        return _GetActionByKeyChordInternal(keys) == nullptr;
    }

    // Method Description:
    // - Retrieves the assigned command that can be invoked with the given key chord
    // Arguments:
    // - keys: the key chord of the command to search for
    // Return Value:
    // - the command with the given key chord
    // - nullptr if the key chord doesn't exist
    Model::Command ActionMap::GetActionByKeyChord(const Control::KeyChord& keys) const
    {
        return _GetActionByKeyChordInternal(keys).value_or(nullptr);
    }

    Model::Command ActionMap::GetActionByID(const winrt::hstring& cmdID) const
    {
        return _GetActionByID(cmdID);
    }

    // Method Description:
    // - Retrieves the assigned command ID with the given key chord.
    // - Can return nullopt to differentiate explicit unbinding vs lack of binding.
    // Arguments:
    // - keys: the key chord of the command to search for
    // Return Value:
    // - the command ID with the given key chord
    // - an empty string if the key chord is explicitly unbound
    // - nullopt if it is not bound
    std::optional<winrt::hstring> ActionMap::_GetActionIdByKeyChordInternal(const Control::KeyChord& keys) const
    {
        if (const auto keyIDPair = _KeyMap.find(keys); keyIDPair != _KeyMap.end())
        {
            // the keychord is defined in this layer, return the ID
            return keyIDPair->second;
        }

        // search through our parents
        for (const auto& parent : _parents)
        {
            if (const auto foundCmdID = parent->_GetActionIdByKeyChordInternal(keys))
            {
                return foundCmdID;
            }
        }

        // we did not find the keychord anywhere, it's not bound and not explicitly unbound either
        return std::nullopt;
    }

    // Method Description:
    // - Retrieves the assigned command with the given key chord.
    // - Can return nullopt to differentiate explicit unbinding vs lack of binding.
    // Arguments:
    // - keys: the key chord of the command to search for
    // Return Value:
    // - the command with the given key chord
    // - nullptr if the key chord is explicitly unbound
    // - nullopt if it is not bound
    std::optional<Model::Command> ActionMap::_GetActionByKeyChordInternal(const Control::KeyChord& keys) const
    {
        if (const auto actionIDOptional = _GetActionIdByKeyChordInternal(keys))
        {
            if (!actionIDOptional->empty())
            {
                // there is an ID associated with these keys, find the command
                if (const auto foundCmd = _GetActionByID(*actionIDOptional))
                {
                    return foundCmd;
                }
            }
            // the ID is an empty string, these keys are explicitly unbound
            return nullptr;
        }

        return std::nullopt;
    }

    // Method Description:
    // - Retrieves the key chord for the provided action
    // Arguments:
    // - cmdID: the ID of the command we're looking for
    // Return Value:
    // - the key chord that executes the given action
    // - nullptr if the action is not bound to a key chord
    Control::KeyChord ActionMap::GetKeyBindingForAction(const winrt::hstring& cmdID)
    {
        if (!_ResolvedKeyToActionMapCache)
        {
            _RefreshKeyBindingCaches();
        }

        if (_CumulativeActionToKeyMapCache.contains(cmdID))
        {
            return _CumulativeActionToKeyMapCache.at(cmdID);
        }

        // This key binding does not exist
        return nullptr;
    }

    IVector<Control::KeyChord> ActionMap::AllKeyBindingsForAction(const winrt::hstring& cmdID)
    {
        if (!_ResolvedKeyToActionMapCache)
        {
            _RefreshKeyBindingCaches();
        }

        std::vector<Control::KeyChord> keybindingsList;
        for (const auto& [key, ID] : _CumulativeKeyToActionMapCache)
        {
            if (ID == cmdID)
            {
                keybindingsList.emplace_back(key);
            }
        }
        return single_threaded_vector(std::move(keybindingsList));
    }

    // Method Description:
    // - Rebinds a key binding to a new key chord
    // Arguments:
    // - oldKeys: the key binding that we are rebinding
    // - newKeys: the new key chord that is being used to replace oldKeys
    // Return Value:
    // - true, if successful; otherwise, false.
    bool ActionMap::RebindKeys(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys)
    {
        const auto cmd{ GetActionByKeyChord(oldKeys) };
        if (!cmd)
        {
            // oldKeys must be bound. Otherwise, we don't know what action to bind.
            return false;
        }

        if (auto oldKeyPair = _KeyMap.find(oldKeys); oldKeyPair != _KeyMap.end())
        {
            // oldKeys is bound in our layer, replace it with newKeys
            _KeyMap.insert_or_assign(newKeys, cmd.ID());
            _KeyMap.erase(oldKeyPair);
        }
        else
        {
            // oldKeys is bound in some other layer, set newKeys to cmd in this layer, and oldKeys to unbound in this layer
            _KeyMap.insert_or_assign(newKeys, cmd.ID());
            _KeyMap.insert_or_assign(oldKeys, L"");
        }

        return true;
    }

    // Method Description:
    // - Unbind a key chord
    // Arguments:
    // - keys: the key chord that is being unbound
    // Return Value:
    // - <none>
    void ActionMap::DeleteKeyBinding(const KeyChord& keys)
    {
        if (auto keyPair = _KeyMap.find(keys); keyPair != _KeyMap.end())
        {
            // this keychord is bound in our layer, delete it
            _KeyMap.erase(keyPair);
        }

        // either the keychord was never in this layer or we just deleted it above,
        // if GetActionByKeyChord still returns a command that means the keychord is bound in another layer
        if (GetActionByKeyChord(keys))
        {
            // set to unbound in this layer
            _KeyMap.emplace(keys, L"");
        }
    }

    void ActionMap::AddKeyBinding(Control::KeyChord keys, const winrt::hstring& cmdID)
    {
        _KeyMap.insert_or_assign(keys, cmdID);
        _changeLog.emplace(KeysKey);
        _RefreshKeyBindingCaches();
    }

    // Method Description:
    // - Add a new key binding
    // - If the key chord is already in use, the conflicting command is overwritten.
    // Arguments:
    // - keys: the key chord that is being bound
    // - action: the action that the keys are being bound to
    // Return Value:
    // - <none>
    void ActionMap::RegisterKeyBinding(Control::KeyChord keys, Model::ActionAndArgs action)
    {
        auto cmd{ make_self<Command>() };
        cmd->ActionAndArgs(action);
        cmd->GenerateID();
        AddAction(*cmd, keys);
    }

    void ActionMap::DeleteUserCommand(const winrt::hstring& cmdID)
    {
        _ActionMap.erase(cmdID);
        _RefreshKeyBindingCaches();
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareSchemeNames(const ColorScheme& lhs, const ColorScheme& rhs)
    {
        std::wstring leftName{ lhs.Name() };
        std::wstring rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    void ActionMap::ExpandCommands(const IVectorView<Model::Profile>& profiles,
                                   const IMapView<winrt::hstring, Model::ColorScheme>& schemes)
    {
        // TODO in review - It's a little weird to stash the expanded commands
        // into a separate map. Is it possible to just replace the name map with
        // the post-expanded commands?
        //
        // WHILE also making sure that upon re-saving the commands, we don't
        // actually serialize the results of the expansion. I don't think it is.

        std::vector<Model::ColorScheme> sortedSchemes;
        sortedSchemes.reserve(schemes.Size());

        for (const auto& nameAndScheme : schemes)
        {
            sortedSchemes.push_back(nameAndScheme.Value());
        }
        std::sort(sortedSchemes.begin(),
                  sortedSchemes.end(),
                  _compareSchemeNames);

        auto copyOfCommands = winrt::single_threaded_map<winrt::hstring, Model::Command>();

        const auto& commandsToExpand{ NameMap() };
        for (auto nameAndCommand : commandsToExpand)
        {
            copyOfCommands.Insert(nameAndCommand.Key(), nameAndCommand.Value());
        }

        implementation::Command::ExpandCommands(copyOfCommands,
                                                profiles,
                                                winrt::param::vector_view<Model::ColorScheme>{ sortedSchemes });

        _ExpandedCommandsCache = winrt::single_threaded_vector<Model::Command>();
        for (const auto& [_, command] : copyOfCommands)
        {
            _ExpandedCommandsCache.Append(command);
        }
    }
    IVector<Model::Command> ActionMap::ExpandedCommands()
    {
        return _ExpandedCommandsCache;
    }

    void ActionMap::ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver)
    {
        for (const auto& [_, cmd] : _ActionMap)
        {
            winrt::get_self<implementation::Command>(cmd)->ResolveMediaResourcesWithBasePath(basePath, resolver);
        }

        // Serialize all nested Command objects added in the current layer
        for (const auto& [_, cmd] : _NestedCommands)
        {
            winrt::get_self<implementation::Command>(cmd)->ResolveMediaResourcesWithBasePath(basePath, resolver);
        }

        // Serialize all iterable Command objects added in the current layer
        for (const auto& cmd : _IterableCommands)
        {
            winrt::get_self<implementation::Command>(cmd)->ResolveMediaResourcesWithBasePath(basePath, resolver);
        }
    }

#pragma region Snippets
    std::vector<Model::Command> _filterToSnippets(IMapView<hstring, Model::Command> nameMap,
                                                  winrt::hstring currentCommandline,
                                                  const std::vector<Model::Command>& localCommands)
    {
        std::vector<Model::Command> results{};

        const auto numBackspaces = currentCommandline.size();
        // Helper to clone a sendInput command into a new Command, with the
        // input trimmed to account for the currentCommandline
        auto createInputAction = [&](const Model::Command& command) -> Model::Command {
            winrt::com_ptr<implementation::Command> cmdImpl;
            cmdImpl.copy_from(winrt::get_self<implementation::Command>(command));

            const auto inArgs{ command.ActionAndArgs().Args().try_as<Model::SendInputArgs>() };
            const auto inputString{ inArgs ? inArgs.Input() : L"" };
            auto args = winrt::make_self<SendInputArgs>(
                winrt::hstring{ fmt::format(FMT_COMPILE(L"{:\x7f^{}}{}"),
                                            L"",
                                            numBackspaces,
                                            inputString) });
            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, *args };

            auto copy = cmdImpl->Copy();
            copy->ActionAndArgs(actionAndArgs);

            if (!copy->HasName())
            {
                // Here, we want to manually generate a send input name, but
                // without visualizing space and backspace
                //
                // This is exactly the body of SendInputArgs::GenerateName, but
                // with visualize_nonspace_control_codes instead of
                // visualize_control_codes, to make filtering in the suggestions
                // UI easier.

                const auto escapedInput = til::visualize_nonspace_control_codes(std::wstring{ inputString });
                const auto name = RS_fmt(L"SendInputCommandKey", escapedInput);
                copy->Name(winrt::hstring{ name });
            }

            return *copy;
        };

        // Helper to copy this command into a snippet-styled command, and any
        // nested commands
        const auto addCommand = [&](auto& command) {
            // If this is not a nested command, and it's a sendInput command...
            if (!command.HasNestedCommands() &&
                command.ActionAndArgs().Action() == ShortcutAction::SendInput)
            {
                // copy it into the results.
                results.push_back(createInputAction(command));
            }
            // If this is nested...
            else if (command.HasNestedCommands())
            {
                // Look for any sendInput commands nested underneath us
                std::vector<Model::Command> empty{};
                auto innerResults = winrt::single_threaded_vector<Model::Command>(_filterToSnippets(command.NestedCommands(), currentCommandline, empty));

                if (innerResults.Size() > 0)
                {
                    // This command did have at least one sendInput under it

                    // Create a new Command, which is a copy of this Command,
                    // which only has SendInputs in it
                    winrt::com_ptr<implementation::Command> cmdImpl;
                    cmdImpl.copy_from(winrt::get_self<implementation::Command>(command));
                    auto copy = cmdImpl->Copy();
                    copy->NestedCommands(innerResults.GetView());

                    results.push_back(*copy);
                }
            }
        };

        // iterate over all the commands in all our actions...
        for (auto&& [_, command] : nameMap)
        {
            addCommand(command);
        }
        // ... and all the local commands passed in here
        for (const auto& command : localCommands)
        {
            addCommand(command);
        }

        return results;
    }

    void ActionMap::AddSendInputAction(winrt::hstring name, winrt::hstring input, const Control::KeyChord keys)
    {
        auto newAction = winrt::make<ActionAndArgs>();
        newAction.Action(ShortcutAction::SendInput);
        auto sendInputArgs = winrt::make<SendInputArgs>(input);
        newAction.Args(sendInputArgs);
        auto cmd{ make_self<Command>() };
        if (!name.empty())
        {
            cmd->Name(name);
        }
        cmd->ActionAndArgs(newAction);
        cmd->GenerateID();
        AddAction(*cmd, keys);
    }

    void ActionMap::UpdateCommandID(const Model::Command& cmd, winrt::hstring newID)
    {
        const auto oldID = cmd.ID();
        if (newID.empty())
        {
            // if the new ID is empty, that means we need to generate a new one
            newID = winrt::get_self<implementation::ActionAndArgs>(cmd.ActionAndArgs())->GenerateID();
        }
        if (newID != oldID)
        {
            if (const auto foundCmd{ _GetActionByID(newID) })
            {
                const auto foundCmdActionAndArgs = foundCmd.ActionAndArgs();
                if (foundCmdActionAndArgs != cmd.ActionAndArgs())
                {
                    // we found a command that has the same ID as this one, but that command has different ActionAndArgs
                    // this means that foundCommand's action and/or args have been changed since its ID was generated,
                    // generate a new one for it
                    // Note: this is recursive! We're calling UpdateCommandID again wich lands us back in here to resolve any cascading collisions
                    auto foundCmdNewID = winrt::get_self<implementation::ActionAndArgs>(foundCmdActionAndArgs)->GenerateID();
                    UpdateCommandID(foundCmd, foundCmdNewID);
                }
            }
            winrt::get_self<implementation::Command>(cmd)->ID(newID);
            // update _ActionMap with the ID change
            _ActionMap.erase(oldID);
            _ActionMap.emplace(newID, cmd);

            // update _KeyMap so that all keys that pointed to the old ID now point to the new ID
            std::vector<KeyChord> keysToRemap;
            for (const auto& [keys, cmdID] : _KeyMap)
            {
                if (cmdID == oldID)
                {
                    keysToRemap.push_back(keys);
                }
            }
            for (const auto& keys : keysToRemap)
            {
                _KeyMap.erase(keys);
                _KeyMap.emplace(keys, newID);
            }
        }
        _RefreshKeyBindingCaches();
    }

    // Look for a .wt.json file in the given directory. If it exists,
    // read it, parse it's JSON, and retrieve all the sendInput actions.
    std::unordered_map<hstring, Model::Command> ActionMap::_loadLocalSnippets(const std::filesystem::path& currentWorkingDirectory)
    {
        // This returns an empty string if we fail to load the file.
        std::filesystem::path localSnippetsPath = currentWorkingDirectory / std::filesystem::path{ ".wt.json" };
        const auto data = til::io::read_file_as_utf8_string_if_exists(localSnippetsPath);
        if (data.empty())
        {
            return {};
        }

        Json::Value root;
        std::string errs;
        const std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            // In the real settings parser, we'd throw here:
            // throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
            //
            // That seems overly aggressive for something that we don't
            // really own. Instead, just bail out.
            return {};
        }

        std::unordered_map<hstring, Model::Command> result;
        if (auto actions{ root[JsonKey("snippets")] })
        {
            for (const auto& json : actions)
            {
                const auto snippet = Command::FromSnippetJson(json);
                result.insert_or_assign(snippet->Name(), *snippet);
            }
        }
        return result;
    }

    winrt::Windows::Foundation::IAsyncOperation<IVector<Model::Command>> ActionMap::FilterToSnippets(
        winrt::hstring currentCommandline,
        winrt::hstring currentWorkingDirectory)
    {
        // enumerate all the parent directories we want to import snippets from
        std::filesystem::path directory{ std::wstring_view{ currentWorkingDirectory } };
        std::vector<std::filesystem::path> directories;
        while (!directory.empty())
        {
            directories.push_back(directory);
            auto parentPath = directory.parent_path();
            if (directory == parentPath)
            {
                break;
            }
            directory = std::move(parentPath);
        }

        {
            // Check if all the directories are already in the cache
            const auto& cache{ _cwdLocalSnippetsCache.lock_shared() };
            if (std::ranges::all_of(directories, [&](auto&& dir) { return cache->contains(dir); }))
            {
                // Load snippets from directories in reverse order.
                // This ensures that we prioritize snippets closer to the cwd.
                // The map makes it easy to avoid duplicates.
                std::unordered_map<hstring, Model::Command> localSnippetsMap;
                for (auto rit = directories.rbegin(); rit != directories.rend(); ++rit)
                {
                    // register snippets from cache
                    for (const auto& [name, snippet] : cache->at(*rit))
                    {
                        localSnippetsMap.insert_or_assign(name, snippet);
                    }
                }

                std::vector<Model::Command> localSnippets;
                localSnippets.reserve(localSnippetsMap.size());
                std::ranges::transform(localSnippetsMap,
                                       std::back_inserter(localSnippets),
                                       [](const auto& kvPair) { return kvPair.second; });
                co_return winrt::single_threaded_vector<Model::Command>(_filterToSnippets(NameMap(),
                                                                                          currentCommandline,
                                                                                          localSnippets));
            }
        } // release the lock on the cache

        // Don't do I/O on the main thread
        co_await winrt::resume_background();

        // Load snippets from directories in reverse order.
        // This ensures that we prioritize snippets closer to the cwd.
        // The map makes it easy to avoid duplicates.
        const auto& cache{ _cwdLocalSnippetsCache.lock() };
        std::unordered_map<hstring, Model::Command> localSnippetsMap;
        for (auto rit = directories.rbegin(); rit != directories.rend(); ++rit)
        {
            const auto& dir = *rit;
            if (const auto cacheIterator = cache->find(dir); cacheIterator != cache->end())
            {
                // register snippets from cache
                for (const auto& [name, snippet] : cache->at(*rit))
                {
                    localSnippetsMap.insert_or_assign(name, snippet);
                }
            }
            else
            {
                // we don't have this directory in the cache, so we need to load it
                auto result = _loadLocalSnippets(dir);
                cache->insert_or_assign(dir, result);

                // register snippets from cache
                std::ranges::for_each(result, [&localSnippetsMap](const auto& kvPair) {
                    localSnippetsMap.insert_or_assign(kvPair.first, kvPair.second);
                });
            }
        }

        std::vector<Model::Command> localSnippets;
        localSnippets.reserve(localSnippetsMap.size());
        std::ranges::transform(localSnippetsMap,
                               std::back_inserter(localSnippets),
                               [](const auto& kvPair) { return kvPair.second; });
        co_return winrt::single_threaded_vector<Model::Command>(_filterToSnippets(NameMap(),
                                                                                  currentCommandline,
                                                                                  localSnippets));
    }
#pragma endregion
}
