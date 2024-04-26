// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AllShortcutActions.h"
#include "ActionMap.h"
#include "Command.h"
#include "AllShortcutActions.h"

#include "ActionMap.g.cpp"

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
#undef ON_ALL_ACTIONS_WITH_ARGS
            default:
                break;
            }

            hasher = til::hasher{ hash };
        }

        hasher.write(action);
        return hasher.finalize();
    }

    bool ActionMap::FixUpsAppliedDuringLoad() const
    {
        return _fixUpsAppliedDuringLoad;
    }

    // Method Description:
    // - Retrieves the Command in the current layer, if it's valid
    // - We internally store invalid commands as full commands.
    //    This helper function returns nullptr when we get an invalid
    //    command. This allows us to simply check for null when we
    //    want a valid command.
    // Arguments:
    // - actionID: the internal ID associated with a Command
    // Return Value:
    // - If the command is valid, the command itself.
    // - If the command is explicitly unbound, nullptr.
    // - If the command cannot be found in this layer, nullopt.
    std::optional<Model::Command> ActionMap::_GetActionByID(const InternalActionID actionID) const
    {
        // Check the masking actions
        const auto maskingPair{ _MaskingActions.find(actionID) };
        if (maskingPair != _MaskingActions.end())
        {
            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(maskingPair->second);

            // masking actions cannot contain nested or invalid commands,
            // so we can just return it directly.
            return maskingPair->second;
        }

        // Check current layer
        const auto actionMapPair{ _ActionMap.find(actionID) };
        if (actionMapPair != _ActionMap.end())
        {
            auto& cmd{ actionMapPair->second };

            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(cmd);

            return !cmd.HasNestedCommands() && cmd.ActionAndArgs().Action() == ShortcutAction::Invalid ?
                       nullptr : // explicitly unbound
                       cmd;
        }

        // We don't have an answer
        return std::nullopt;
    }

    std::optional<Model::Command> ActionMap::_GetActionByID2(const winrt::hstring actionID) const
    {
        // Check current layer
        const auto actionMapPair{ _ActionMap2.find(actionID) };
        if (actionMapPair != _ActionMap2.end())
        {
            auto& cmd{ actionMapPair->second };

            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(cmd);

            // todo: stage 3 - not sure if we need this? _ActionMap2 doesn't contain invalid commands I'm p sure
            return !cmd.HasNestedCommands() && cmd.ActionAndArgs().Action() == ShortcutAction::Invalid ?
                       nullptr : // explicitly unbound
                       cmd;
        }

        // We don't have an answer
        return std::nullopt;
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
        // todo: stage 2 (done)
        // todo: stage 3 - can we update RegisterShortcutAction to use new IDs instead of InternalActionID?
        //                 then we'll be able to update _PopulateAvailableACtionsWithStandardCommands with new IDs too
        //                 the problem is that we need IDs for every ShortcutAction, even if it is not in defaults/user settings
        if (!_AvailableActionsCache)
        {
            // populate _AvailableActionsCache
            std::unordered_map<hstring, Model::ActionAndArgs> availableActions;
            std::unordered_set<InternalActionID> visitedActionIDs;
            _PopulateAvailableActionsWithStandardCommands(availableActions, visitedActionIDs);

// now add any ShortcutActions that we might have missed
#define ON_ALL_ACTIONS(action) RegisterShortcutAction(ShortcutAction::action, availableActions, visitedActionIDs);
            ALL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS

            _AvailableActionsCache = single_threaded_map(std::move(availableActions));
        }
        if (!_AvailableActionsCache2)
        {
            // populate _AvailableActionsCache
            std::unordered_map<hstring, Model::ActionAndArgs> availableActions2;
            std::unordered_set<InternalActionID> visitedActionIDs2;
            _PopulateAvailableActionsWithStandardCommands2(availableActions2, visitedActionIDs2);

// now add any ShortcutActions that we might have missed
#define ON_ALL_ACTIONS(action) RegisterShortcutAction(ShortcutAction::action, availableActions2, visitedActionIDs2);
            ALL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS

            _AvailableActionsCache2 = single_threaded_map(std::move(availableActions2));
        }
        return _AvailableActionsCache2.GetView();
    }

    void ActionMap::_PopulateAvailableActionsWithStandardCommands(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const
    {
        // Update AvailableActions and visitedActionIDs with our current layer
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            if (cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
            {
                // Only populate AvailableActions with actions that haven't been visited already.
                if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
                {
                    const auto& name{ cmd.Name() };
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
        }

        // Update NameMap and visitedActionIDs with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateAvailableActionsWithStandardCommands(availableActions, visitedActionIDs);
        }
    }

    void ActionMap::_PopulateAvailableActionsWithStandardCommands2(std::unordered_map<hstring, Model::ActionAndArgs>& availableActions, std::unordered_set<InternalActionID>& visitedActionIDs) const
    {
        // Update AvailableActions and visitedActionIDs with our current layer
        for (const auto& [_, cmd] : _ActionMap2)
        {
            // todo: stage 3 - not sure if we need this? _ActionMap2 doesn't contain invalid commands anymore I'm p sure
            if (cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
            {
                // Only populate AvailableActions with actions that haven't been visited already.
                const auto actionID = Hash(cmd.ActionAndArgs());
                if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
                {
                    const auto& name{ cmd.Name() };
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
        }

        // Update NameMap and visitedActionIDs with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateAvailableActionsWithStandardCommands2(availableActions, visitedActionIDs);
        }
    }

    // Method Description:
    // - Retrieves a map of command names to the commands themselves
    // - These commands should not be modified directly because they may result in
    //    an invalid state for the `ActionMap`
    IMapView<hstring, Model::Command> ActionMap::NameMap()
    {
        // todo: stage 1 (done)
        if (!_NameMapCache)
        {
            // populate _NameMapCache
            std::unordered_map<hstring, Model::Command> nameMap{};
            _PopulateNameMapWithSpecialCommands(nameMap);
            _PopulateNameMapWithStandardCommands(nameMap);

            _NameMapCache = single_threaded_map(std::move(nameMap));
        }
        if (!_NameMapCache2)
        {
            // populate _NameMapCache
            std::unordered_map<hstring, Model::Command> nameMap2{};
            _PopulateNameMapWithSpecialCommands2(nameMap2);
            _PopulateNameMapWithStandardCommands2(nameMap2);

            _NameMapCache2 = single_threaded_map(std::move(nameMap2));
        }
        return _NameMapCache2.GetView();
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
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    //             There should only ever by one of each command (identified by the actionID) in the nameMap.
    void ActionMap::_PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        std::unordered_set<InternalActionID> visitedActionIDs;
        for (const auto& cmd : _GetCumulativeActions())
        {
            // only populate with valid commands
            if (cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
            {
                // Only populate NameMap with actions that haven't been visited already.
                const auto actionID{ Hash(cmd.ActionAndArgs()) };
                if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
                {
                    const auto& name{ cmd.Name() };
                    if (!name.empty())
                    {
                        // Update NameMap.
                        nameMap.insert_or_assign(name, cmd);
                    }

                    // Record that we already handled adding this action to the NameMap.
                    visitedActionIDs.emplace(actionID);
                }
            }
        }
    }

    // Method Description:
    // - Populates the provided nameMap with all of our special commands and our parent's special commands.
    // - Special commands include nested and iterable commands.
    // - Performs a top-down approach by going to the root first, then recursively adding the nested commands layer-by-layer.
    // Arguments:
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    void ActionMap::_PopulateNameMapWithSpecialCommands2(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        // Update NameMap with our parents.
        // Starting with this means we're doing a top-down approach.
        for (const auto& parent : _parents)
        {
            parent->_PopulateNameMapWithSpecialCommands2(nameMap);
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
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    //             There should only ever by one of each command (identified by the actionID) in the nameMap.
    void ActionMap::_PopulateNameMapWithStandardCommands2(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        std::unordered_set<winrt::hstring> visitedActionIDs;
        for (const auto& cmd : _GetCumulativeActions2())
        {
            // only populate with valid commands
            if (cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
            {
                // Only populate NameMap with actions that haven't been visited already.
                const auto actionID{ cmd.ID() };
                if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
                {
                    const auto& name{ cmd.Name() };
                    if (!name.empty())
                    {
                        // Update NameMap.
                        nameMap.insert_or_assign(name, cmd);
                    }

                    // Record that we already handled adding this action to the NameMap.
                    visitedActionIDs.emplace(actionID);
                }
            }
        }
    }

    // Method Description:
    // - Provides an accumulated list of actions that are exposed. The accumulated list includes actions added in this layer, followed by actions added by our parents.
    std::vector<Model::Command> ActionMap::_GetCumulativeActions() const noexcept
    {
        // First, add actions from our current layer
        std::vector<Model::Command> cumulativeActions;
        cumulativeActions.reserve(_MaskingActions.size() + _ActionMap.size());

        // masking actions have priority. Actions here are constructed from consolidating an inherited action with changes we've found when populating this layer.
        std::transform(_MaskingActions.begin(), _MaskingActions.end(), std::back_inserter(cumulativeActions), [](std::pair<InternalActionID, Model::Command> actionPair) {
            return actionPair.second;
        });
        std::transform(_ActionMap.begin(), _ActionMap.end(), std::back_inserter(cumulativeActions), [](std::pair<InternalActionID, Model::Command> actionPair) {
            return actionPair.second;
        });

        // Now, add the accumulated actions from our parents
        for (const auto& parent : _parents)
        {
            const auto parentActions{ parent->_GetCumulativeActions() };
            cumulativeActions.reserve(cumulativeActions.size() + parentActions.size());
            cumulativeActions.insert(cumulativeActions.end(), parentActions.begin(), parentActions.end());
        }

        return cumulativeActions;
    }

    // Method Description:
    // - Provides an accumulated list of actions that are exposed. The accumulated list includes actions added in this layer, followed by actions added by our parents.
    std::vector<Model::Command> ActionMap::_GetCumulativeActions2() const noexcept
    {
        // check this
        // First, add actions from our current layer
        std::vector<Model::Command> cumulativeActions;
        cumulativeActions.reserve(_ActionMap2.size());

        std::transform(_ActionMap2.begin(), _ActionMap2.end(), std::back_inserter(cumulativeActions), [](std::pair<winrt::hstring, Model::Command> actionPair) {
            return actionPair.second;
        });

        // Now, add the accumulated actions from our parents
        for (const auto& parent : _parents)
        {
            const auto parentActions{ parent->_GetCumulativeActions2() };
            cumulativeActions.reserve(cumulativeActions.size() + parentActions.size());
            cumulativeActions.insert(cumulativeActions.end(), parentActions.begin(), parentActions.end());
        }

        return cumulativeActions;
    }

    IMapView<Control::KeyChord, Model::Command> ActionMap::GlobalHotkeys()
    {
        // todo: stage 1 (done)
        if (!_GlobalHotkeysCache)
        {
            _RefreshKeyBindingCaches();
        }
        if (!_GlobalHotkeysCache2)
        {
            _RefreshKeyBindingCaches2();
        }
        return _GlobalHotkeysCache2.GetView();
    }

    IMapView<Control::KeyChord, Model::Command> ActionMap::KeyBindings()
    {
        // todo: stage 1 (done)
        if (!_KeyBindingMapCache)
        {
            _RefreshKeyBindingCaches();
        }
        if (!_KeyBindingMapCache2)
        {
            _RefreshKeyBindingCaches2();
        }
        return _KeyBindingMapCache2.GetView();
    }

    void ActionMap::_RefreshKeyBindingCaches()
    {
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> keyBindingsMap;
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> globalHotkeys;
        std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality> unboundKeys;

        _PopulateKeyBindingMapWithStandardCommands(keyBindingsMap, unboundKeys);

        for (const auto& [keys, cmd] : keyBindingsMap)
        {
            // Only populate GlobalHotkeys with actions whose
            // ShortcutAction is GlobalSummon or QuakeMode
            if (cmd.ActionAndArgs().Action() == ShortcutAction::GlobalSummon || cmd.ActionAndArgs().Action() == ShortcutAction::QuakeMode)
            {
                globalHotkeys.emplace(keys, cmd);
            }
        }

        _KeyBindingMapCache = single_threaded_map(std::move(keyBindingsMap));
        _GlobalHotkeysCache = single_threaded_map(std::move(globalHotkeys));
    }

    void ActionMap::_RefreshKeyBindingCaches2()
    {
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> keyBindingsMap;
        std::unordered_map<KeyChord, Model::Command, KeyChordHash, KeyChordEquality> globalHotkeys;
        std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality> unboundKeys;

        _PopulateKeyBindingMapWithStandardCommands2(keyBindingsMap, unboundKeys);

        for (const auto& [keys, cmd] : keyBindingsMap)
        {
            // Only populate GlobalHotkeys with actions whose
            // ShortcutAction is GlobalSummon or QuakeMode
            if (cmd.ActionAndArgs().Action() == ShortcutAction::GlobalSummon || cmd.ActionAndArgs().Action() == ShortcutAction::QuakeMode)
            {
                globalHotkeys.emplace(keys, cmd);
            }
        }

        _KeyBindingMapCache2 = single_threaded_map(std::move(keyBindingsMap));
        _GlobalHotkeysCache2 = single_threaded_map(std::move(globalHotkeys));
    }

    // Method Description:
    // - Populates the provided keyBindingsMap with all of our actions and our parents actions
    //    while omitting the key bindings that were already added before.
    // - This needs to be a bottom up approach to ensure that we only add each key chord once.
    // Arguments:
    // - keyBindingsMap: the keyBindingsMap we're populating. This maps the key chord of a command to the command itself.
    // - unboundKeys: a set of keys that are explicitly unbound
    void ActionMap::_PopulateKeyBindingMapWithStandardCommands(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const
    {
        // Update KeyBindingsMap with our current layer
        for (const auto& [keys, actionID] : _KeyMap)
        {
            // Get the action our KeyMap maps to.
            // This _cannot_ be nullopt because KeyMap can only map to
            //   actions in this layer.
            // This _can_ be nullptr because nullptr means it was
            //   explicitly unbound ( "command": "unbound", "keys": "ctrl+c" ).
            const auto cmd{ _GetActionByID(actionID).value() };
            if (cmd)
            {
                // iterate over all of the action's bound keys
                const auto cmdImpl{ get_self<Command>(cmd) };
                for (const auto& keys : cmdImpl->KeyMappings())
                {
                    // Only populate KeyBindingsMap with actions that...
                    // (1) haven't been visited already
                    // (2) aren't explicitly unbound
                    if (keyBindingsMap.find(keys) == keyBindingsMap.end() && unboundKeys.find(keys) == unboundKeys.end())
                    {
                        keyBindingsMap.emplace(keys, cmd);
                    }
                }
            }
            else
            {
                // record any keys that are explicitly unbound,
                // but don't add them to the list of key bindings
                unboundKeys.emplace(keys);
            }
        }

        // Update keyBindingsMap and unboundKeys with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateKeyBindingMapWithStandardCommands(keyBindingsMap, unboundKeys);
        }
    }

    // Method Description:
    // - Populates the provided keyBindingsMap with all of our actions and our parents actions
    //    while omitting the key bindings that were already added before.
    // - This needs to be a bottom up approach to ensure that we only add each key chord once.
    // Arguments:
    // - keyBindingsMap: the keyBindingsMap we're populating. This maps the key chord of a command to the command itself.
    // - unboundKeys: a set of keys that are explicitly unbound
    void ActionMap::_PopulateKeyBindingMapWithStandardCommands2(std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality>& keyBindingsMap, std::unordered_set<Control::KeyChord, KeyChordHash, KeyChordEquality>& unboundKeys) const
    {
        // Update KeyBindingsMap with our current layer
        for (const auto& [keys, actionID] : _KeyMap2)
        {
            // Get the action our KeyMap maps to.
            // If the actionID is empty, this keybinding is explicitly unbound
            if (!actionID.empty())
            {
                const auto cmd{ _GetActionByID2(actionID) };
                if (cmd.has_value())
                {
                    // iterate over all of the action's bound keys
                    const auto cmdImpl{ get_self<Command>(cmd.value()) };
                    for (const auto& kc : cmdImpl->KeyMappings())
                    {
                        // Only populate KeyBindingsMap with actions that...
                        // (1) haven't been visited already
                        // (2) aren't explicitly unbound
                        if (keyBindingsMap.find(kc) == keyBindingsMap.end() && unboundKeys.find(kc) == unboundKeys.end())
                        {
                            keyBindingsMap.emplace(kc, cmd.value());
                        }
                    }
                }
            }
            else
            {
                // record any keys that are explicitly unbound,
                // but don't add them to the list of key bindings
                unboundKeys.emplace(keys);
            }
        }

        // Update keyBindingsMap and unboundKeys with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateKeyBindingMapWithStandardCommands2(keyBindingsMap, unboundKeys);
        }
    }

    com_ptr<ActionMap> ActionMap::Copy() const
    {
        auto actionMap{ make_self<ActionMap>() };

        // KeyChord --> ID
        actionMap->_KeyMap = _KeyMap;
        actionMap->_KeyMap2 = _KeyMap2;

        // ID --> Command
        actionMap->_ActionMap.reserve(_ActionMap.size());
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            actionMap->_ActionMap.emplace(actionID, *winrt::get_self<Command>(cmd)->Copy());
        }

        // ID --> Command
        actionMap->_ActionMap2.reserve(_ActionMap2.size());
        for (const auto& [actionID, cmd] : _ActionMap2)
        {
            actionMap->_ActionMap2.emplace(actionID, *winrt::get_self<Command>(cmd)->Copy());
        }

        // ID --> Command
        actionMap->_MaskingActions.reserve(_MaskingActions.size());
        for (const auto& [actionID, cmd] : _MaskingActions)
        {
            actionMap->_MaskingActions.emplace(actionID, *winrt::get_self<Command>(cmd)->Copy());
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
    void ActionMap::AddAction(const Model::Command& cmd)
    {
        // _Never_ add null to the ActionMap
        if (!cmd)
        {
            return;
        }

        // invalidate caches
        _NameMapCache = nullptr;
        _GlobalHotkeysCache = nullptr;
        _KeyBindingMapCache = nullptr;
        _NameMapCache2 = nullptr;
        _GlobalHotkeysCache2 = nullptr;
        _KeyBindingMapCache2 = nullptr;

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
        //  Add the new command to the KeyMap.
        //  This map directs you to an entry in the ActionMap.

        // Removing Actions from the Command Palette:
        //  cmd.Name and cmd.Action have a one-to-one relationship.
        //  If cmd.Name is empty, we must retrieve the old name and remove it.

        // Removing Key Bindings:
        //  cmd.Keys and cmd.Action have a many-to-one relationship.
        //  If cmd.Keys is empty, we don't care.
        //  If action is "unbound"/"invalid", you're explicitly unbinding the provided cmd.keys.
        //  NOTE: If we're unbinding a command from a different layer, we must use maskingActions
        //         to keep track of what key mappings are still valid.

        // _TryUpdateActionMap may update oldCmd and maskingCmd

        //Model::Command oldCmd{ nullptr };
        //Model::Command maskingCmd{ nullptr };
        //_TryUpdateActionMap(cmd, oldCmd, maskingCmd);

        //_TryUpdateName(cmd, oldCmd, maskingCmd);
        //_TryUpdateKeyChord(cmd, oldCmd, maskingCmd);

        _TryUpdateActionMap2(cmd);
        // I don't think we need a _TryUpdateName with the new implementation?
        // we might still need it for legacy case...
        _TryUpdateKeyChord2(cmd);
    }

    // Method Description:
    // - Try to add the new command to _ActionMap.
    // - If the command was added previously in this layer, populate oldCmd.
    // - If the command was added previously in another layer, populate maskingCmd.
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action found in _ActionMap, if one already exists
    // - maskingAction: the action found in a parent layer, if one already exists
    void ActionMap::_TryUpdateActionMap(const Model::Command& cmd, Model::Command& oldCmd, Model::Command& maskingCmd)
    {
        // Example:
        //   { "command": "copy", "keys": "ctrl+c" }       --> add the action in for the first time
        //   { "command": "copy", "keys": "ctrl+shift+c" } --> update oldCmd
        const auto actionID{ Hash(cmd.ActionAndArgs()) };
        const auto& actionPair{ _ActionMap.find(actionID) };
        if (actionPair == _ActionMap.end())
        {
            // add this action in for the first time
            _ActionMap.emplace(actionID, cmd);
        }
        else
        {
            // We're adding an action that already exists in our layer.
            // Record it so that we update it with any new information.
            oldCmd = actionPair->second;
        }

        // Masking Actions
        //
        // Example:
        //   parent:  { "command": "copy", "keys": "ctrl+c" }       --> add the action to parent._ActionMap
        //   current: { "command": "copy", "keys": "ctrl+shift+c" } --> look through parents for the "ctrl+c" binding, add it to _MaskingActions
        //            { "command": "copy", "keys": "ctrl+ins" }     --> this should already be in _MaskingActions

        // Now check if this action was introduced in another layer.
        const auto& maskingActionPair{ _MaskingActions.find(actionID) };
        if (maskingActionPair == _MaskingActions.end())
        {
            // Check if we need to add this to our list of masking commands.
            for (const auto& parent : _parents)
            {
                // NOTE: This only checks the layer above us, but that's ok.
                //       If we had to find one from a layer above that, parent->_MaskingActions
                //       would have found it, so we inherit it for free!
                const auto& inheritedCmd{ parent->_GetActionByID(actionID) };
                if (inheritedCmd && *inheritedCmd)
                {
                    const auto& inheritedCmdImpl{ get_self<Command>(*inheritedCmd) };
                    maskingCmd = *inheritedCmdImpl->Copy();
                    _MaskingActions.emplace(actionID, maskingCmd);
                }
            }
        }
        else
        {
            // This is an action that we already have a mutable "masking" record for.
            // Record it so that we update it with any new information.
            maskingCmd = maskingActionPair->second;
        }
    }

    void ActionMap::_TryUpdateActionMap2(const Model::Command& cmd)
    {
        // todo: stage 1 (done)

        // only add to the _ActionMap if there is an ID and the shortcut action is valid
        // (if the shortcut action is invalid, then this is for unbinding and _TryUpdateKeyChord will handle that)
        if (auto cmdID = cmd.ID(); !cmdID.empty() && cmd.ActionAndArgs().Action() != ShortcutAction::Invalid)
        {
            // if a command with cmdID already exists, we need to port the keybindings over and then overwrite the previous command
            // explanation by example:
            // - command object with ID "X" already exists, and has "ctrl" in its keymappings
            // - this means our _KeyMap has an entry for "ctrl" that points to "X"
            // - we are now adding a new command object that also has ID "X", and has "shift" in its keymappings
            // - _ActionMap needs to be updated so that "X" points to new command object
            // - however _KeyMap will now contain "ctrl" -> "X" and also "shift" -> "X"
            // - so we have to let this command object know that "ctrl" points to it as well
            if (const auto idCmdPair = _ActionMap2.find(cmdID); idCmdPair != _ActionMap2.end())
            {
                const auto newCmdImpl{ get_self<implementation::Command>(cmd) };
                const auto oldCmdImpl{ get_self<implementation::Command>(idCmdPair->second) };

                // Command's keymapping implementation cares about order (the keychord at the back of the vector
                // is considered its 'primary' keychord) - make sure we preserve this order as we register the old command's keys

                // make a copy of the new commands keymappings (we will insert them later to preserve order)
                // todo: stage 3 - can we make this cleaner omg
                std::vector<KeyChord> newCmdKeymappingsCopy;
                for (const auto kc : newCmdImpl->KeyMappings())
                {
                    newCmdKeymappingsCopy.emplace_back(kc);
                }

                // copy the old commands key mappings into the new command
                for (const auto kc : oldCmdImpl->KeyMappings())
                {
                    newCmdImpl->RegisterKey(kc);
                }

                // transfer back the commands from the copy we made
                // (this just makes sure that newCmd's keys are at the back of the vector,
                // duplicates are automatically deleted by RegisterKey)
                for (const auto kc : newCmdKeymappingsCopy)
                {
                    newCmdImpl->RegisterKey(kc);
                }
            }

            _ActionMap2.insert_or_assign(cmdID, cmd);
        }
    }

    // Method Description:
    // - Update our internal state with the name of the newly registered action
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action that already exists in our internal state. May be null.
    // - maskingCmd: the masking action that already exists in our internal state. May be null.
    void ActionMap::_TryUpdateName(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& maskingCmd)
    {
        // Example:
        //   { "name": "foo", "command": "copy" } --> we are setting a name, update oldCmd and maskingCmd
        //   {                "command": "copy" } --> no change to name, exit early
        const auto cmdImpl{ get_self<Command>(cmd) };
        if (!cmdImpl->HasName())
        {
            // the user is not trying to update the name.
            return;
        }

        // Update oldCmd:
        //   If we have a Command in our _ActionMap that we're trying to update,
        //   update it.
        const auto newName{ cmd.Name() };
        if (oldCmd)
        {
            // This command has a name, check if it's new.
            if (newName != oldCmd.Name())
            {
                // The new name differs from the old name,
                // update our name.
                auto oldCmdImpl{ get_self<Command>(oldCmd) };
                oldCmdImpl->Name(newName);
            }
        }

        // Update maskingCmd:
        //   We have a Command that is masking one from a parent layer.
        //   We need to ensure that this has the correct name. That way,
        //    we can return an accumulated view of a Command at this layer.
        //   This differs from oldCmd which is mainly used for serialization
        //    by recording the delta of the Command in this layer.
        if (maskingCmd)
        {
            // This command has a name, check if it's new.
            if (newName != maskingCmd.Name())
            {
                // The new name differs from the old name,
                // update our name.
                auto maskingCmdImpl{ get_self<Command>(maskingCmd) };
                maskingCmdImpl->Name(newName);
            }
        }

        // Handle a collision with NestedCommands
        _NestedCommands.erase(newName);
    }

    // Method Description:
    // - Update our internal state with the key chord of the newly registered action
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action that already exists in our internal state. May be null.
    // - maskingCmd: the masking action that already exists in our internal state. May be null.
    void ActionMap::_TryUpdateKeyChord(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& maskingCmd)
    {
        // Example:
        //   {                "command": "copy", "keys": "ctrl+c" } --> we are registering a new key chord, update oldCmd and maskingCmd
        //   { "name": "foo", "command": "copy" }                   --> no change to keys, exit early
        const auto keys{ cmd.Keys() };
        if (!keys)
        {
            // the user is not trying to update the keys.
            return;
        }

        // Handle collisions
        const auto oldKeyPair{ _KeyMap.find(keys) };
        if (oldKeyPair != _KeyMap.end())
        {
            // Collision: The key chord was already in use.
            //
            // Example:
            //   { "command": "copy",  "keys": "ctrl+c" } --> register "ctrl+c" (different branch)
            //   { "command": "paste", "keys": "ctrl+c" } --> Collision! (this branch)
            //
            // Remove the old one. (unbind "copy" in the example above)
            const auto actionPair{ _ActionMap.find(oldKeyPair->second) };
            const auto conflictingCmd{ actionPair->second };
            const auto conflictingCmdImpl{ get_self<implementation::Command>(conflictingCmd) };
            conflictingCmdImpl->EraseKey(keys);
        }
        else if (const auto& conflictingCmd{ GetActionByKeyChord(keys) })
        {
            // Collision with ancestor: The key chord was already in use, but by an action in another layer
            //
            // Example:
            //   parent:  { "command": "copy",    "keys": "ctrl+c" } --> register "ctrl+c" (different branch)
            //   current: { "command": "paste",   "keys": "ctrl+c" } --> Collision with ancestor! (this branch, sub-branch 1)
            //            { "command": "unbound", "keys": "ctrl+c" } --> Collision with masking action! (this branch, sub-branch 2)
            const auto conflictingActionID{ Hash(conflictingCmd.ActionAndArgs()) };
            const auto maskingCmdPair{ _MaskingActions.find(conflictingActionID) };
            if (maskingCmdPair == _MaskingActions.end())
            {
                // This is the first time we're colliding with an action from a different layer,
                //   so let's add this action to _MaskingActions and update it appropriately.
                // Create a copy of the conflicting action,
                //   and erase the conflicting key chord from the copy.
                const auto conflictingCmdImpl{ get_self<implementation::Command>(conflictingCmd) };
                const auto conflictingCmdCopy{ conflictingCmdImpl->Copy() };
                conflictingCmdCopy->EraseKey(keys);
                _MaskingActions.emplace(conflictingActionID, *conflictingCmdCopy);
            }
            else
            {
                // We've collided with this action before. Let's resolve a collision with a masking action.
                const auto maskingCmdImpl{ get_self<implementation::Command>(maskingCmdPair->second) };
                maskingCmdImpl->EraseKey(keys);
            }
        }

        // Assign the new action in the _KeyMap.
        const auto actionID{ Hash(cmd.ActionAndArgs()) };
        _KeyMap.insert_or_assign(keys, actionID);

        // Additive operation:
        //   Register the new key chord with oldCmd (an existing _ActionMap entry)
        // Example:
        //   { "command": "copy",  "keys": "ctrl+c" }       --> register "ctrl+c" (section above)
        //   { "command": "copy",  "keys": "ctrl+shift+c" } --> also register "ctrl+shift+c" to the same Command (oldCmd)
        if (oldCmd)
        {
            // Update inner Command with new key chord
            auto oldCmdImpl{ get_self<Command>(oldCmd) };
            oldCmdImpl->RegisterKey(keys);
        }

        // Additive operation:
        //   Register the new key chord with maskingCmd (an existing _maskingAction entry)
        // Example:
        //   parent:  { "command": "copy", "keys": "ctrl+c" }       --> register "ctrl+c" to parent._ActionMap (different branch in a different layer)
        //   current: { "command": "copy", "keys": "ctrl+shift+c" } --> also register "ctrl+shift+c" to the same Command (maskingCmd)
        if (maskingCmd)
        {
            // Update inner Command with new key chord
            auto maskingCmdImpl{ get_self<Command>(maskingCmd) };
            maskingCmdImpl->RegisterKey(keys);
        }
    }

    void ActionMap::_TryUpdateKeyChord2(const Model::Command& cmd)
    {
        // Example (this is a legacy case, where the keys are provided in the same block as the command):
        //   {                "command": "copy", "keys": "ctrl+c" } --> we are registering a new key chord
        //   { "name": "foo", "command": "copy" }                   --> no change to keys, exit early
        const auto keys{ cmd.Keys() };
        if (!keys)
        {
            // the user is not trying to update the keys.
            return;
        }

        // Handle collisions
        const auto oldKeyPair{ _KeyMap2.find(keys) };
        if (oldKeyPair != _KeyMap2.end())
        {
            // Collision: The key chord was already in use.
            //
            // Example:
            //   { "command": "copy",  "keys": "ctrl+c" } --> register "ctrl+c" (different branch)
            //   { "command": "paste", "keys": "ctrl+c" } --> Collision! (this branch)
            //
            // Remove the old one. (unbind "copy" in the example above)

            // if oldKeyPair->second is empty, that means this keychord was unbound earlier in this layer and is now being rebound
            // no collision logic needed - we will simply reassign it in the _KeyMap
            if (!oldKeyPair->second.empty())
            {
                const auto actionPair{ _ActionMap2.find(oldKeyPair->second) };
                const auto conflictingCmd{ actionPair->second };
                const auto conflictingCmdImpl{ get_self<implementation::Command>(conflictingCmd) };
                conflictingCmdImpl->EraseKey(keys);
            }
        }

        // Assign the new action in the _KeyMap
        // However, there's a strange edge case here - since we're parsing a legacy or modern block,
        // the user might have { "command": null, "id": "someID", "keys": "ctrl+c" }
        // i.e. they provided an ID for a null command (which they really shouldn't, there's no purpose)
        // in this case, we do _not_ want to use the id they provided, we want to use an empty id
        // (empty id in the _KeyMap indicates the the keychord was explicitly unbound)
        if (cmd.ActionAndArgs().Action() == ShortcutAction::Invalid)
        {
            _KeyMap2.insert_or_assign(keys, L"");
        }
        else
        {
            _KeyMap2.insert_or_assign(keys, cmd.ID());   
        }

        cmd.RegisterKey(keys);
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
        // todo: stage 1 (done)
        return _GetActionByKeyChordInternal2(keys) == nullptr;
        //return _GetActionByKeyChordInternal(keys) == nullptr;
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
        // todo: stage 1 (done)
        return _GetActionByKeyChordInternal2(keys).value_or(nullptr);
        //return _GetActionByKeyChordInternal(keys).value_or(nullptr);
    }

    // Method Description:
    // - Retrieves the assigned command with the given key chord.
    // - Can return nullopt to differentiate explicit unbinding vs lack of binding.
    // Arguments:
    // - keys: the key chord of the command to search for
    // Return Value:
    // - the command with the given key chord
    // - nullptr if the key chord is explicitly unbound
    // - nullopt if it was not bound in this layer
    std::optional<Model::Command> ActionMap::_GetActionByKeyChordInternal(const Control::KeyChord& keys) const
    {
        // Check the current layer
        if (const auto actionIDPair = _KeyMap.find(keys); actionIDPair != _KeyMap.end())
        {
            // the command was explicitly bound,
            // return what we found (invalid commands exposed as nullptr)
            return _GetActionByID(actionIDPair->second);
        }

        // the command was not bound in this layer,
        // ask my parents
        for (const auto& parent : _parents)
        {
            const auto& inheritedCmd{ parent->_GetActionByKeyChordInternal(keys) };
            if (inheritedCmd)
            {
                return *inheritedCmd;
            }
        }

        // This action is not explicitly bound
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
    // - nullopt if it was not bound in this layer
    std::optional<Model::Command> ActionMap::_GetActionByKeyChordInternal2(const Control::KeyChord& keys) const
    {
        // todo: stage 3 - why does this function search through parents but _GetActionByID2 doesn't? even the original implementation was similar with some recursive and some non-recursive functions
        if (const auto keyIDPair = _KeyMap2.find(keys); keyIDPair != _KeyMap2.end())
        {
            if (const auto cmdID = keyIDPair->second; !cmdID.empty())
            {
                if (const auto cmd = _GetActionByID2(cmdID); cmd.has_value())
                {
                    // standard case: both the keys and the ID are defined in this layer
                    return cmd;
                }
                else
                {
                    for (const auto parent : _parents)
                    {
                        if (const auto inheritedCmd = parent->_GetActionByID2(cmdID); inheritedCmd.has_value())
                        {
                            // edge case 1: the keys are bound to an ID in this layer, but the ID is defined in one of our parents
                            return inheritedCmd;
                        }
                    }
                }
            }
            else
            {
                // the keychord is defined in this layer, but points to an empty string - explicitly unbound
                return nullptr;
            }
        }

        // search through our parents
        for (const auto& parent : _parents)
        {
            if (const auto parentKeyIDPair = parent->_KeyMap2.find(keys); parentKeyIDPair != parent->_KeyMap2.end())
            {
                if (const auto cmdID = parentKeyIDPair->second; !cmdID.empty())
                {
                    if (const auto cmd = _GetActionByID2(cmdID); cmd.has_value())
                    {
                        // edge case 2: the keychord maps to an ID in one of our parents, but a command with that ID exists in this layer
                        //              use the command from this layer
                        return cmd;
                    }
                }
            }

            // we've checked for the standard case and the 2 edge cases, now we can recurse
            const auto& inheritedCmd{ parent->_GetActionByKeyChordInternal2(keys) };
            if (inheritedCmd)
            {
                return *inheritedCmd;
            }
        }

        // we did not find the keychord anywhere, its not bound and not explicity unbound either
        return std::nullopt;
    }

    // Method Description:
    // - Retrieves the key chord for the provided action
    // Arguments:
    // - action: the shortcut action (an action type) we're looking for
    // Return Value:
    // - the key chord that executes the given action
    // - nullptr if the action is not bound to a key chord
    Control::KeyChord ActionMap::GetKeyBindingForAction(const ShortcutAction& action) const
    {
        return GetKeyBindingForAction(action, nullptr);
    }

    // Method Description:
    // - Retrieves the key chord for the provided action
    // Arguments:
    // - action: the shortcut action (an action type) we're looking for
    // - myArgs: the action args for the action we're looking for
    // Return Value:
    // - the key chord that executes the given action
    // - nullptr if the action is not bound to a key chord
    Control::KeyChord ActionMap::GetKeyBindingForAction(const ShortcutAction& myAction, const IActionArgs& myArgs) const
    {
        if (myAction == ShortcutAction::Invalid)
        {
            return nullptr;
        }

        // Check our internal state.
        const auto actionAndArgs = winrt::make<ActionAndArgs>(myAction, myArgs);
        const auto hash{ Hash(actionAndArgs) };
        if (const auto& cmd{ _GetActionByID(hash) })
        {
            return cmd->Keys();
        }

        // Check our parents
        for (const auto& parent : _parents)
        {
            if (const auto& keys{ parent->GetKeyBindingForAction(myAction, myArgs) })
            {
                return keys;
            }
        }

        // This key binding does not exist
        return nullptr;
    }

    // Method Description:
    // - Retrieves the key chord for the provided action
    // Arguments:
    // - cmdID: the ID of the command we're looking for
    // Return Value:
    // - the key chord that executes the given action
    // - nullptr if the action is not bound to a key chord
    Control::KeyChord ActionMap::GetKeyBindingForAction2(winrt::hstring cmdID) const
    {
        // todo: stage 1 (done, need to update the callers to use this, note for review: what if the user makes an action that does the same thing and edits the keys?)
        // Check our internal state.
        if (const auto& cmd{ _GetActionByID2(cmdID) })
        {
            return cmd->Keys();
        }

        // Check our parents
        for (const auto& parent : _parents)
        {
            if (const auto& keys{ parent->GetKeyBindingForAction2(cmdID) })
            {
                return keys;
            }
        }

        // This key binding does not exist
        return nullptr;
    }

    bool ActionMap::GenerateIDsForActions()
    {
        bool fixedUp{ false };
        for (auto actionPair : _ActionMap)
        {
            auto cmdImpl{ winrt::get_self<Command>(actionPair.second) };

            // Note: this function should ONLY be called for the action map in the user's settings file
            //       this debug assert should verify that for debug builds
            assert(cmdImpl->Origin() == OriginTag::User);

            if (cmdImpl->ID().empty())
            {
                fixedUp = cmdImpl->GenerateID() || fixedUp;
            }
        }
        _fixUpsAppliedDuringLoad = true;
        // todo: stage 3 - probably don't need this as a return value anymore?
        return fixedUp;
    }

    // Method Description:
    // - Rebinds a key binding to a new key chord
    // Arguments:
    // - oldKeys: the key binding that we are rebinding
    // - newKeys: the new key chord that is being used to replace oldKeys
    // Return Value:
    // - true, if successful. False, otherwise.
    bool ActionMap::RebindKeys(const Control::KeyChord& oldKeys, const Control::KeyChord& newKeys)
    {
        // todo: stage 2 (done)
        const auto cmd{ GetActionByKeyChord(oldKeys) };
        if (!cmd)
        {
            // oldKeys must be bound. Otherwise, we don't know what action to bind.
            return false;
        }

        //if (newKeys)
        //{
        //    // Bind newKeys
        //    const auto newCmd{ make_self<Command>() };
        //    newCmd->ActionAndArgs(cmd.ActionAndArgs());
        //    newCmd->RegisterKey(newKeys);
        //    AddAction(*newCmd);
        //}

        //// unbind oldKeys
        //DeleteKeyBinding(oldKeys);

        // possible cases:
        // - keybinding exists in our layer and the parent layer
        //      REPLACE oldKeys with newKeys
        // - keybinding only exists in our layer
        //      REPLACE oldKeys with newKeys
        // - keybinding only exists in parent layer
        //      ADD newKeys to our map
        if (auto oldKeyPair = _KeyMap2.find(oldKeys); oldKeyPair != _KeyMap2.end())
        {
            // oldKeys is bound in our layer, replace it with newKeys
            _KeyMap2.insert_or_assign(newKeys, cmd.ID());
            _KeyMap2.erase(oldKeyPair);
        }
        else
        {
            // oldKeys is bound in some other layer, just set newKeys in this layer
            _KeyMap2.insert_or_assign(newKeys, cmd.ID());
        }

        // make sure to update the Command with these changes
        const auto cmdImpl{ get_self<implementation::Command>(cmd) };
        cmdImpl->EraseKey(oldKeys);
        cmdImpl->RegisterKey(newKeys);

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
        // create an "unbound" command
        // { "command": "unbound", "keys": <keys> }
        // todo: stage 2 (done)
        //const auto cmd{ make_self<Command>() };
        //cmd->ActionAndArgs(make<ActionAndArgs>());
        //cmd->RegisterKey(keys);
        //AddAction(*cmd);

        // possible cases:
        // - keys exist in our layer and parent layer
        //      convert the mapping in our layer to unbound
        // - keys only exist in our layer
        //      just delete from our map
        // - keys only exist in parent layer
        //      make an unbound command in this layer
        if (auto keyPair = _KeyMap2.find(keys); keyPair != _KeyMap2.end())
        {
            // this keychord is bound in our layer, delete it
            _KeyMap2.erase(keyPair);
        }

        // either the keychord was never in this layer or we just deleted it above,
        // if GetActionByKeyChord still returns a command that means the keychord is bound in another layer
        if (GetActionByKeyChord(keys))
        {
            // set to unbound in this layer
            _KeyMap2.emplace(keys, L"");
        }
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
        // todo: stage 2 (done)
        auto cmd{ make_self<Command>() };
        cmd->RegisterKey(keys);
        cmd->ActionAndArgs(action);
        cmd->GenerateID();
        AddAction(*cmd);
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

    IVector<Model::Command> _filterToSendInput(IMapView<hstring, Model::Command> nameMap,
                                               winrt::hstring currentCommandline)
    {
        auto results = winrt::single_threaded_vector<Model::Command>();

        const auto numBackspaces = currentCommandline.size();
        // Helper to clone a sendInput command into a new Command, with the
        // input trimmed to account for the currentCommandline
        auto createInputAction = [&](const Model::Command& command) -> Model::Command {
            winrt::com_ptr<implementation::Command> cmdImpl;
            cmdImpl.copy_from(winrt::get_self<implementation::Command>(command));

            const auto inArgs{ command.ActionAndArgs().Args().try_as<Model::SendInputArgs>() };

            auto args = winrt::make_self<SendInputArgs>(
                winrt::hstring{ fmt::format(FMT_COMPILE(L"{:\x7f^{}}{}"),
                                            L"",
                                            numBackspaces,
                                            (std::wstring_view)(inArgs ? inArgs.Input() : L"")) });
            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, *args };

            auto copy = cmdImpl->Copy();
            copy->ActionAndArgs(actionAndArgs);

            return *copy;
        };

        // iterate over all the commands in all our actions...
        for (auto&& [name, command] : nameMap)
        {
            // If this is not a nested command, and it's a sendInput command...
            if (!command.HasNestedCommands() &&
                command.ActionAndArgs().Action() == ShortcutAction::SendInput)
            {
                // copy it into the results.
                results.Append(createInputAction(command));
            }
            // If this is nested...
            else if (command.HasNestedCommands())
            {
                // Look for any sendInput commands nested underneath us
                auto innerResults = _filterToSendInput(command.NestedCommands(), currentCommandline);

                if (innerResults.Size() > 0)
                {
                    // This command did have at least one sendInput under it

                    // Create a new Command, which is a copy of this Command,
                    // which only has SendInputs in it
                    winrt::com_ptr<implementation::Command> cmdImpl;
                    cmdImpl.copy_from(winrt::get_self<implementation::Command>(command));
                    auto copy = cmdImpl->Copy();
                    copy->NestedCommands(innerResults.GetView());

                    results.Append(*copy);
                }
            }
        }

        return results;
    }

    IVector<Model::Command> ActionMap::FilterToSendInput(
        winrt::hstring currentCommandline)
    {
        return _filterToSendInput(NameMap(), currentCommandline);
    }
}
