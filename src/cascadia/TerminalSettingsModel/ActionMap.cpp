// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionMap.h"

#include "ActionMap.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ActionMap::ActionMap() :
        _NestedCommands{ single_threaded_map<hstring, Model::Command>() }
    {
    }

    // Method Description:
    // - Retrieves the Command in the ActionList, if it's valid
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
    std::optional<Model::Command> ActionMap::_GetActionByID(InternalActionID actionID) const
    {
        const auto& cmdPair{ _ActionMap.find(actionID) };
        if (cmdPair == _ActionMap.end())
        {
            // we don't have an answer
            return std::nullopt;
        }
        else
        {
            const auto& cmd{ cmdPair->second };

            // ActionMap should never point to nullptr
            FAIL_FAST_IF_NULL(cmd);

            return !cmd.HasNestedCommands() && cmd.ActionAndArgs().Action() == ShortcutAction::Invalid ?
                       nullptr : // explicitly unbound
                       cmd;
        }
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
            std::set<InternalActionID> visitedActionIDs{ ActionHash()(make<implementation::ActionAndArgs>()) };
            _PopulateNameMapWithNestedCommands(nameMap);
            _PopulateNameMapWithStandardCommands(nameMap, visitedActionIDs);

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
    // - This needs to be a bottom up approach to ensure that we only add each action (identified by action ID) once.
    // Arguments:
    // - nameMap: the nameMap we're populating. This maps the name (hstring) of a command to the command itself.
    //             There should only ever by one of each command (identified by the actionID) in the nameMap.
    // - visitedActionIDs: the actionIDs that we've already added to the nameMap. Commands with a matching actionID
    //                      have already been added, and should be ignored.
    void ActionMap::_PopulateNameMapWithStandardCommands(std::unordered_map<hstring, Model::Command>& nameMap, std::set<InternalActionID>& visitedActionIDs) const
    {
        // Update NameMap and visitedActionIDs with our current layer
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            // Only populate NameMap with actions that haven't been visited already.
            if (visitedActionIDs.find(actionID) == visitedActionIDs.end())
            {
                const auto& name{ cmd.Name() };
                if (!name.empty())
                {
                    // Update NameMap.
                    nameMap.insert_or_assign(name, cmd);
                }

                // Record that we already handled adding this action to the NameMap.
                visitedActionIDs.insert(actionID);
            }
        }

        // Update NameMap and visitedActionIDs with our parents
        for (const auto& parent : _parents)
        {
            parent->_PopulateNameMapWithStandardCommands(nameMap, visitedActionIDs);
        }
    }

    com_ptr<ActionMap> ActionMap::Copy() const
    {
        auto actionMap{ make_self<ActionMap>() };

        // copy _KeyMap (KeyChord --> ID)
        std::for_each(_KeyMap.begin(), _KeyMap.end(), [actionMap](const auto& pair) {
            actionMap->_KeyMap.insert(pair);
        });

        // copy _ActionMap (ID --> Command)
        for (const auto& [actionID, cmd] : _ActionMap)
        {
            actionMap->_ActionMap.insert({ actionID, *(get_self<Command>(cmd)->Copy()) });
        }

        // copy _NestedCommands (Name --> Command)
        for (const auto& [name, cmd] : _NestedCommands)
        {
            actionMap->_NestedCommands.Insert(name, *(get_self<Command>(cmd)->Copy()));
        }

        // repeat this for each of our parents
        for (const auto& parent : _parents)
        {
            actionMap->_parents.push_back(std::move(parent->Copy()));
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
        //  Add the new command to the NameMap and KeyMap (whichever is applicable).
        //  These maps direct you to an entry in the ActionMap.

        // Removing Actions from the Command Palette:
        //  cmd.Name and cmd.Action have a one-to-one relationship.
        //  If cmd.Name is empty, we must retrieve the old name and remove it.

        // Removing Key Bindings:
        //  cmd.Keys and cmd.Action have a many-to-one relationship.
        //  If cmd.Keys is empty, we don't care.
        //  If action is "unbound"/"invalid", you're explicitly unbinding the provided cmd.keys.

        Model::Command oldCmd{ nullptr };
        const auto actionID{ ActionHash()(cmd.ActionAndArgs()) };
        auto actionPair{ _ActionMap.find(actionID) };
        if (actionPair != _ActionMap.end())
        {
            // We're adding an action that already exists.
            // Record it and update it with any new information.
            oldCmd = actionPair->second;

            // Update Name
            const auto cmdImpl{ get_self<Command>(cmd) };
            if (cmdImpl->HasName())
            {
                // This command has a name, check if it's new.
                const auto newName{ cmd.Name() };
                if (newName != oldCmd.Name())
                {
                    // The new name differs from the old name,
                    // update our name.
                    auto oldCmdImpl{ get_self<Command>(oldCmd) };
                    oldCmdImpl->Name(newName);
                }

                // Handle a collision with NestedCommands
                _NestedCommands.TryRemove(newName);
            }
        }
        else
        {
            // add this action in for the first time
            _ActionMap.insert({ actionID, cmd });

            // Handle a collision with NestedCommands
            _NestedCommands.TryRemove(cmd.Name());
        }

        // Update KeyMap
        if (const auto keys{ cmd.Keys() })
        {
            const auto oldKeyPair{ _KeyMap.find(keys) };
            if (oldKeyPair != _KeyMap.end())
            {
                // Collision: The key chord was already in use.
                // Remove the old one.
                actionPair = _ActionMap.find(oldKeyPair->second);
                const auto& conflictingCmd{ actionPair->second };
                const auto& conflictingCmdImpl{ get_self<implementation::Command>(conflictingCmd) };
                conflictingCmdImpl->EraseKey(keys);
            }

            // Assign the new action.
            _KeyMap.insert_or_assign(keys, actionID);

            if (oldCmd)
            {
                // Update inner Command with new key chord
                auto oldCmdImpl{ get_self<Command>(oldCmd) };
                oldCmdImpl->RegisterKey(keys);
            }
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
        const auto& cmd{ _GetActionByKeyChordInternal(keys) };
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
        const auto hash{ ActionHash()(actionAndArgs) };
        if (const auto& cmd{ _GetActionByID(hash) })
        {
            return cmd.value().Keys();
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
}
