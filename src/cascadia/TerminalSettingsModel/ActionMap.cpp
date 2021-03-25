// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionMap.h"

#include "ActionMap.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // Method Description:
    // - Retrieves the Command in the ActionList, if it's valid
    // - We internally store invalid commands as full commands.
    //    This helper function returns nullptr when we get an invalid
    //    command. This allows us to simply check for null when we
    //    want a valid command.
    // Arguments:
    // - actionID: the internal ID associated with a Command
    // Return Value:
    // - If the command is valid, the command itself. Otherwise, null.
    Model::Command ActionMap::_GetActionByID(size_t actionID) const
    {
        // ActionMap is not being maintained appropriately
        //  the given ID points out of bounds
        FAIL_FAST_IF(actionID >= _ActionList.size());

        const auto& cmd{ til::at(_ActionList, actionID) };

        // ActionList should never point to nullptr
        FAIL_FAST_IF_NULL(cmd);

        return !cmd.HasNestedCommands() && cmd.Action().Action() == ShortcutAction::Invalid ?
                   nullptr :
                   cmd;
    }

    bool ActionMap::_IsActionIdValid(size_t actionID) const
    {
        // ActionMap is not being maintained appropriately
        //  the given ID points out of bounds
        FAIL_FAST_IF(actionID >= _ActionList.size());

        const auto& cmd{ til::at(_ActionList, actionID) };

        // ActionList should never point to nullptr
        FAIL_FAST_IF_NULL(cmd);

        return cmd.HasNestedCommands() || cmd.Action().Action() != ShortcutAction::Invalid;
    }

    // Method Description:
    // - Retrieves a map of command names to the commands themselves
    // - These commands should not be modified directly because they may result in
    //    an invalid state for the `ActionMap`
    Windows::Foundation::Collections::IMapView<hstring, Model::Command> ActionMap::NameMap() const
    {
        // add everything from our parents
        auto map{ single_threaded_map<hstring, Model::Command>() };
        for (const auto& parent : _parents)
        {
            const auto& inheritedNameMap{ parent->NameMap() };
            for (const auto& [key, val] : inheritedNameMap)
            {
                map.Insert(key, val);
            }
        }

        // add everything from our layer
        for (const auto& [name, actionID] : _NameMap)
        {
            if (const auto& cmd{ _GetActionByID(actionID) })
            {
                map.Insert(name, cmd);
            }
        }
        return map.GetView();
    }

    com_ptr<ActionMap> ActionMap::Copy() const
    {
        auto actionMap{ make_self<ActionMap>() };
        std::for_each(_NameMap.begin(), _NameMap.end(), [actionMap](const auto& pair) {
            actionMap->_NameMap.insert(pair);
        });

        std::for_each(_KeyMap.begin(), _KeyMap.end(), [actionMap](const auto& pair) {
            actionMap->_KeyMap.insert(pair);
        });

        std::for_each(_ActionList.begin(), _ActionList.end(), [actionMap](const auto& cmd) {
            const auto& cmdCopy{ get_self<Command>(cmd)->Copy() };
            actionMap->_ActionList.push_back(*cmdCopy);
        });
        return actionMap;
    }

    // Method Description:
    // - [Recursive] The number of key bindings the ActionMap has
    //    access to.
    size_t ActionMap::KeybindingCount() const noexcept
    {
        // NOTE: We cannot do _KeyMap.size() here because
        //       unbinding keys works by adding an "unbound"/"invalid"
        //       action to the KeyMap and ActionList.
        //       Thus, we must check the validity of each action in our KeyMap.

        // count the key bindings in our layer
        size_t count{ 0 };
        for (const auto& [_, actionID] : _KeyMap)
        {
            if (_IsActionIdValid(actionID))
            {
                ++count;
            }
        }

        // count the key bindings in our parents
        for (const auto& parent : _parents)
        {
            count += parent->KeybindingCount();
        }
        return count;
    }

    // Method Description:
    // - [Recursive] The number of commands the ActionMap has
    //    access to.
    size_t ActionMap::CommandCount() const noexcept
    {
        // count the named commands in our layer
        size_t count{ 0 };
        for (const auto& [_, actionID] : _NameMap)
        {
            if (_IsActionIdValid(actionID))
            {
                ++count;
            }
        }

        // count the named commands in our parents
        for (const auto& parent : _parents)
        {
            count += parent->CommandCount();
        }
        return count;
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

        // General Case:
        //  Add the new command to the NameMap and KeyMap (if applicable).
        //  These maps direct you to an entry in the ActionList.
        // Unbinding Actions:
        //  Same as above, except the ActionList entry is specifically
        //  an unbound action. This is important for serialization.

        const auto actionID{ _ActionList.size() };
        _ActionList.push_back(cmd);

        // Update NameMap
        const auto name{ cmd.Name() };
        if (!name.empty())
        {
            const auto oldActionPair{ _NameMap.find(name) };
            if (oldActionPair != _NameMap.end())
            {
                // the name was already in use.
                // remove the old one.
                auto& cmd{ til::at(_ActionList, oldActionPair->second) };
                cmd.Name(L"");

                // and add the new one
                _NameMap.insert_or_assign(name, actionID);
            }
            else
            {
                // the name isn't mapped to anything,
                //  insert the actionID.
                _NameMap.insert({ name, actionID });
            }
        }

        // Update KeyMap
        if (const auto keys{ cmd.Keys() })
        {
            const auto oldActionPair{ _KeyMap.find(keys) };
            if (oldActionPair != _KeyMap.end())
            {
                // the key chord was already in use.
                // remove the old one.
                auto& cmd{ til::at(_ActionList, oldActionPair->second) };
                cmd.Keys(nullptr);

                // and add the new one
                _KeyMap.insert_or_assign(keys, actionID);
            }
            else
            {
                // the key chord isn't mapped to anything,
                //  insert the actionID.
                _KeyMap.insert({ keys, actionID });
            }
        }
    }

    // Method Description:
    // - Retrieves the assigned command with the given name
    // Arguments:
    // - name: the name of the command to search for
    // Return Value:
    // - the command with the given name
    // - nullptr if the name is explicitly unbound
    Model::Command ActionMap::GetActionByName(hstring const& name) const
    {
        // Check the current layer
        const auto& cmd{ _GetActionByNameInternal(name) };
        if (cmd.has_value())
        {
            return *cmd;
        }

        // Check our parents
        for (const auto& parent : _parents)
        {
            const auto& inheritedCmd{ parent->_GetActionByNameInternal(name) };
            if (inheritedCmd.has_value())
            {
                return *inheritedCmd;
            }
        }

        // This action does not exist
        return nullptr;
    }

    // Method Description:
    // - Retrieves the assigned command _in this layer_ with the given name
    // Arguments:
    // - name: the name of the command to search for
    // Return Value:
    // - the command with the given name
    // - nullptr if the name is explicitly unbound
    // - nullopt if it was not bound in this layer
    std::optional<Model::Command> ActionMap::_GetActionByNameInternal(hstring const& name) const
    {
        // Check the current layer
        const auto actionIDPair{ _NameMap.find(name) };
        if (actionIDPair != _NameMap.end())
        {
            // the action was explicitly bound,
            // return what we found (invalid commands exposed as nullptr)
            return _GetActionByID(actionIDPair->second);
        }

        // the command was not bound in this layer
        return std::nullopt;
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

        // Check our parents
        for (const auto& parent : _parents)
        {
            const auto& inheritedCmd{ parent->_GetActionByKeyChordInternal(keys) };
            if (inheritedCmd.has_value())
            {
                return *inheritedCmd;
            }
        }

        // This action does not exist
        return nullptr;
    }

    // Method Description:
    // - Retrieves the assigned command _in this layer_ with the given key chord
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

        // the command was not bound in this layer
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
        // Iterate through the list backwards so that we find the most recent change.
        for (auto cmd{ _ActionList.rbegin() }; cmd != _ActionList.rend(); ++cmd)
        {
            // breakdown cmd
            const auto& actionAndArgs{ cmd->Action() };

            if (!actionAndArgs || actionAndArgs.Action() == ShortcutAction::Invalid)
            {
                continue;
            }

            const auto& action{ actionAndArgs.Action() };
            const auto& args{ actionAndArgs.Args() };

            // check if we have a match
            const auto actionMatched{ action == myAction };
            const auto argsMatched{ args ? args.Equals(myArgs) : args == myArgs };
            if (actionMatched && argsMatched)
            {
                return cmd->Keys();
            }
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
