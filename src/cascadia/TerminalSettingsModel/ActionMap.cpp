// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionMap.h"

#include "ActionMap.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    static InternalActionID Hash(const Model::ActionAndArgs& actionAndArgs)
    {
        size_t hashedAction{ HashUtils::HashProperty(actionAndArgs.Action()) };

        size_t hashedArgs{};
        if (const auto& args{ actionAndArgs.Args() })
        {
            hashedArgs = gsl::narrow_cast<size_t>(args.Hash());
        }
        else
        {
            std::hash<IActionArgs> argsHash;
            hashedArgs = argsHash(nullptr);
        }
        return hashedAction ^ hashedArgs;
    }

    ActionMap::ActionMap() :
        _NestedCommands{ single_threaded_map<hstring, Model::Command>() }
    {
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
        // Check the masked actions
        const auto maskedPair{ _MaskedActions.find(actionID) };
        if (maskedPair != _MaskedActions.end())
        {
            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(maskedPair->second);

            // masked actions cannot contain nested or invalid commands,
            // so we can just return it directly.
            return maskedPair->second;
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

    // Method Description:
    // - Retrieves a map of command names to the commands themselves
    // - These commands should not be modified directly because they may result in
    //    an invalid state for the `ActionMap`
    Windows::Foundation::Collections::IMapView<hstring, Model::Command> ActionMap::NameMap()
    {
        if (!_NameMapCache)
        {
            // populate _NameMapCache
            std::unordered_map<hstring, Model::Command> nameMap{};
            _PopulateNameMapWithNestedCommands(nameMap);
            _PopulateNameMapWithStandardCommands(nameMap);

            _NameMapCache = single_threaded_map<hstring, Model::Command>(std::move(nameMap));
        }
        return _NameMapCache.GetView();
    }

    // Method Description:
    // - Populates the provided nameMap with all of our nested commands and our parents nested commands
    // - Performs a top-down approach by going to the root first, then recursively adding the nested commands layer-by-layer
    // Arguments:
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    void ActionMap::_PopulateNameMapWithNestedCommands(std::unordered_map<hstring, Model::Command>& nameMap) const
    {
        // Update NameMap with our parents.
        // Starting with this means we're doing a top-down approach.
        FAIL_FAST_IF(_parents.size() > 1);
        for (const auto& parent : _parents)
        {
            parent->_PopulateNameMapWithNestedCommands(nameMap);
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
            // skip over all invalid actions
            if (cmd.ActionAndArgs().Action() == ShortcutAction::Invalid)
            {
                continue;
            }

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

    // Method Description:
    // - Provides an accumulated list of actions that are exposed. The accumulated list includes actions added in this layer, followed by actions added by our parents.
    std::vector<Model::Command> ActionMap::_GetCumulativeActions() const noexcept
    {
        // First, add actions from our current layer
        std::vector<Model::Command> cumulativeActions;
        cumulativeActions.reserve(_MaskedActions.size() + _ActionMap.size());

        // masked actions have priority. Actions here are constructed from consolidating an inherited action with changes we've found when populating this layer.
        std::transform(_MaskedActions.begin(), _MaskedActions.end(), std::back_inserter(cumulativeActions), [](std::pair<InternalActionID, Model::Command> actionPair) {
            return actionPair.second;
        });
        std::transform(_ActionMap.begin(), _ActionMap.end(), std::back_inserter(cumulativeActions), [](std::pair<InternalActionID, Model::Command> actionPair) {
            return actionPair.second;
        });

        // Now, add the accumulated actions from our parents
        FAIL_FAST_IF(_parents.size() > 1);
        for (const auto& parent : _parents)
        {
            const auto parentActions{ parent->_GetCumulativeActions() };
            cumulativeActions.reserve(cumulativeActions.size() + parentActions.size());
            cumulativeActions.insert(cumulativeActions.end(), parentActions.begin(), parentActions.end());
        }

        return cumulativeActions;
    }

    Windows::Foundation::Collections::IMapView<Control::KeyChord, Model::Command> ActionMap::GlobalHotkeys()
    {
        if (!_GlobalHotkeysCache)
        {
            std::unordered_set<InternalActionID> visitedActionIDs{};
            std::unordered_map<Control::KeyChord, Model::Command, KeyChordHash, KeyChordEquality> globalHotkeys;
            for (const auto& cmd : _GetCumulativeActions())
            {
                // Only populate GlobalHotkeys with actions that...
                // (1) ShortcutAction is GlobalSummon or QuakeMode
                const auto& actionAndArgs{ cmd.ActionAndArgs() };
                if (actionAndArgs.Action() == ShortcutAction::GlobalSummon || actionAndArgs.Action() == ShortcutAction::QuakeMode)
                {
                    // (2) haven't been visited already
                    const auto actionID{ Hash(actionAndArgs) };
                    if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
                    {
                        const auto& cmdImpl{ get_self<Command>(cmd) };
                        for (const auto& keys : cmdImpl->KeyMappings())
                        {
                            // (3) haven't had that key chord added yet
                            if (globalHotkeys.find(keys) == globalHotkeys.end())
                            {
                                globalHotkeys.emplace(keys, cmd);
                            }
                        }

                        // Record that we already handled adding this action to the NameMap.
                        visitedActionIDs.emplace(actionID);
                    }
                }
            }
            _GlobalHotkeysCache = single_threaded_map<Control::KeyChord, Model::Command>(std::move(globalHotkeys));
        }
        return _GlobalHotkeysCache.GetView();
    }

    com_ptr<ActionMap> ActionMap::Copy() const
    {
        auto actionMap{ make_self<ActionMap>() };

        // copy _KeyMap (KeyChord --> ID)
        actionMap->_KeyMap = _KeyMap;

        // copy _ActionMap (ID --> Command)
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            actionMap->_ActionMap.emplace(actionID, *(get_self<Command>(cmd)->Copy()));
        }

        // copy _MaskedActions (ID --> Command)
        for (const auto& [actionID, cmd] : _MaskedActions)
        {
            actionMap->_MaskedActions.emplace(actionID, *(get_self<Command>(cmd)->Copy()));
        }

        // copy _NestedCommands (Name --> Command)
        for (const auto& [name, cmd] : _NestedCommands)
        {
            actionMap->_NestedCommands.Insert(name, *(get_self<Command>(cmd)->Copy()));
        }

        // repeat this for each of our parents
        FAIL_FAST_IF(_parents.size() > 1);
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

        // Handle nested commands
        const auto cmdImpl{ get_self<Command>(cmd) };
        if (cmdImpl->IsNestedCommand())
        {
            // But check if it actually has a name to bind to first
            const auto name{ cmd.Name() };
            if (!name.empty())
            {
                _NestedCommands.Insert(name, cmd);
            }
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
        //  NOTE: If we're unbinding a command from a different layer, we must use maskedActions
        //         to keep track of what key mappings are still valid.

        // _TryUpdateActionMap may update oldCmd and maskedCmd
        Model::Command oldCmd{ nullptr };
        Model::Command maskedCmd{ nullptr };
        _TryUpdateActionMap(cmd, oldCmd, maskedCmd);

        _TryUpdateName(cmd, oldCmd, maskedCmd);
        _TryUpdateKeyChord(cmd, oldCmd, maskedCmd);
    }

    // Method Description:
    // - Try to add the new command to _ActionMap.
    // - If the command was added previously in this layer, populate oldCmd.
    // - If the command was added previously in another layer, populate maskedCmd.
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action found in _ActionMap, if one already exists
    // - maskedAction: the action found in a parent layer, if one already exists
    void ActionMap::_TryUpdateActionMap(const Model::Command& cmd, Model::Command& oldCmd, Model::Command& maskedCmd)
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

        // Masked Actions
        //
        // Example:
        //   parent:  { "command": "copy", "keys": "ctrl+c" }       --> add the action to parent._ActionMap
        //   current: { "command": "copy", "keys": "ctrl+shift+c" } --> look through parents for the "ctrl+c" binding, add it to _MaskedActions
        //            { "command": "copy", "keys": "ctrl+ins" }     --> this should already be in _MaskedActions

        // Now check if this action was introduced in another layer.
        const auto& maskedActionPair{ _MaskedActions.find(actionID) };
        if (maskedActionPair == _MaskedActions.end())
        {
            // Check if we need to add this to our list of masked commands.
            FAIL_FAST_IF(_parents.size() > 1);
            for (const auto& parent : _parents)
            {
                // NOTE: This only checks the layer above us, but that's ok.
                //       If we had to find one from a layer above that, parent->_MaskedActions
                //       would have found it, so we inherit it for free!
                const auto& inheritedCmd{ parent->_GetActionByID(actionID) };
                if (inheritedCmd.has_value() && inheritedCmd.value())
                {
                    const auto& inheritedCmdImpl{ get_self<Command>(inheritedCmd.value()) };
                    maskedCmd = *inheritedCmdImpl->Copy();
                    _MaskedActions.emplace(actionID, maskedCmd);
                }
            }
        }
        else
        {
            // We're adding an action that already existed on a different layer.
            // Record it so that we update it with any new information.
            maskedCmd = maskedActionPair->second;
        }
    }

    // Method Description:
    // - Update our internal state with the name of the newly registered action
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action that already exists in our internal state. May be null.
    // - maskedCmd: the masked action that already exists in our internal state. May be null.
    void ActionMap::_TryUpdateName(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& maskedCmd)
    {
        // Example:
        //   { "name": "foo", "command": "copy" } --> we are setting a name, update oldCmd and maskedCmd
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

        // Update maskedCmd:
        if (maskedCmd)
        {
            // This command has a name, check if it's new.
            if (newName != maskedCmd.Name())
            {
                // The new name differs from the old name,
                // update our name.
                auto maskedCmdImpl{ get_self<Command>(maskedCmd) };
                maskedCmdImpl->Name(newName);
            }
        }

        // Handle a collision with NestedCommands
        _NestedCommands.TryRemove(newName);
    }

    // Method Description:
    // - Update our internal state with the key chord of the newly registered action
    // Arguments:
    // - cmd: the action we're trying to register
    // - oldCmd: the action that already exists in our internal state. May be null.
    // - maskedCmd: the masked action that already exists in our internal state. May be null.
    void ActionMap::_TryUpdateKeyChord(const Model::Command& cmd, const Model::Command& oldCmd, const Model::Command& maskedCmd)
    {
        // Example:
        //   {                "command": "copy", "keys": "ctrl+c" } --> we are registering a new key chord, update oldCmd and maskedCmd
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
            //            { "command": "unbound", "keys": "ctrl+c" } --> Collision with masked action! (this branch, sub-branch 2)
            const auto conflictingActionID{ Hash(conflictingCmd.ActionAndArgs()) };
            const auto maskedCmdPair{ _MaskedActions.find(conflictingActionID) };
            if (maskedCmdPair == _MaskedActions.end())
            {
                // This is the first time we're colliding with an action from a different layer,
                //   so let's add this action to _MaskedActions and update it appropriately.
                // Create a copy of the conflicting action,
                //   and erase the conflicting key chord from the copy.
                const auto conflictingCmdImpl{ get_self<implementation::Command>(conflictingCmd) };
                const auto conflictingCmdCopy{ conflictingCmdImpl->Copy() };
                conflictingCmdCopy->EraseKey(keys);
                _MaskedActions.emplace(conflictingActionID, *conflictingCmdCopy);
            }
            else
            {
                // We've collided with this action before. Let's resolve a collision with a masked action.
                const auto maskedCmdImpl{ get_self<implementation::Command>(maskedCmdPair->second) };
                maskedCmdImpl->EraseKey(keys);
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
        //   Register the new key chord with maskedCmd (an existing _MaskedAction entry)
        // Example:
        //   parent:  { "command": "copy", "keys": "ctrl+c" }       --> register "ctrl+c" to parent._ActionMap (different branch in a different layer)
        //   current: { "command": "copy", "keys": "ctrl+shift+c" } --> also register "ctrl+shift+c" to the same Command (maskedCmd)
        if (maskedCmd)
        {
            // Update inner Command with new key chord
            auto maskedCmdImpl{ get_self<Command>(maskedCmd) };
            maskedCmdImpl->RegisterKey(keys);
        }
    }

    // Method Description:
    // - Retrieves the assigned command that can be invoked with the given key chord
    // Arguments:
    // - keys: the key chord of the command to search for
    // Return Value:
    // - the command with the given key chord
    // - nullptr if the key chord is explicitly unbound
    Model::Command ActionMap::GetActionByKeyChord(Control::KeyChord const& keys) const
    {
        // Check the current layer
        const auto cmd{ _GetActionByKeyChordInternal(keys) };
        if (cmd.has_value())
        {
            return *cmd;
        }

        // This key chord is not explicitly bound
        return nullptr;
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
    std::optional<Model::Command> ActionMap::_GetActionByKeyChordInternal(Control::KeyChord const& keys) const
    {
        // Check the current layer
        const auto actionIDPair{ _KeyMap.find(keys) };
        if (actionIDPair != _KeyMap.end())
        {
            // the command was explicitly bound,
            // return what we found (invalid commands exposed as nullptr)
            return _GetActionByID(actionIDPair->second);
        }

        // the command was not bound in this layer,
        // ask my parents
        FAIL_FAST_IF(_parents.size() > 1);
        for (const auto& parent : _parents)
        {
            const auto& inheritedCmd{ parent->_GetActionByKeyChordInternal(keys) };
            if (inheritedCmd.has_value())
            {
                return *inheritedCmd;
            }
        }

        // This action is not explicitly bound
        return std::nullopt;
    }

    // Method Description:
    // - Retrieves the key chord for the provided action
    // Arguments:
    // - action: the shortcut action (an action type) we're looking for
    // Return Value:
    // - the key chord that executes the given action
    // - nullptr if the action is not bound to a key chord
    Control::KeyChord ActionMap::GetKeyBindingForAction(ShortcutAction const& action) const
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
    Control::KeyChord ActionMap::GetKeyBindingForAction(ShortcutAction const& myAction, IActionArgs const& myArgs) const
    {
        if (myAction == ShortcutAction::Invalid)
        {
            return nullptr;
        }

        // Check our internal state.
        const ActionAndArgs& actionAndArgs{ myAction, myArgs };
        const auto hash{ Hash(actionAndArgs) };
        if (const auto& cmd{ _GetActionByID(hash) })
        {
            return cmd.value().Keys();
        }

        // Check our parents
        FAIL_FAST_IF(_parents.size() > 1);
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
}
