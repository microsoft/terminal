// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// - A couple helper functions for serializing/deserializing a KeyMapping
//   to/from json.
//
// Author(s):
// - Mike Griese - May 2019

#include "pch.h"
#include "ActionMap.h"
#include "ActionAndArgs.h"
#include "KeyChordSerialization.h"
#include "JsonUtils.h"

#include "Command.h"

using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    com_ptr<ActionMap> ActionMap::FromJson(const Json::Value& json, const OriginTag origin)
    {
        auto result = make_self<ActionMap>();
        result->LayerJson(json, origin);
        return result;
    }

    // Method Description:
    // - Deserialize an ActionMap from the array `json`
    // - The json array either contains an array of serialized `Command` objects,
    //   or an array of keybindings
    // - The actions are added to _ActionMap and the keybindings are added to _KeyMap
    // Arguments:
    // - json: an array of Json::Value's to deserialize into our _ActionMap and _KeyMap
    // Return value:
    // - a list of warnings encountered while deserializing the json
    std::vector<SettingsLoadWarnings> ActionMap::LayerJson(const Json::Value& json, const OriginTag origin, const bool withKeybindings)
    {
        // It's possible that the user provided keybindings have some warnings in
        // them - problems that we should alert the user to, but we can recover
        // from. Most of these warnings cannot be detected later in the Validate
        // settings phase, so we'll collect them now.
        std::vector<SettingsLoadWarnings> warnings;

        for (const auto& jsonBlock : json)
        {
            if (!jsonBlock.isObject())
            {
                continue;
            }

            // the json block may be 1 of 3 things:
            // - the legacy style command block, that has the action, args and keys in it
            // - the modern style command block, that has the action, args and an ID
            // - the modern style keys block, that has the keys and an ID

            // if the block contains a "command" field, it is either a legacy or modern style command block
            // and we can call Command::FromJson on it (Command::FromJson can handle parsing both legacy or modern)

            // if there is no "command" field, then it is a modern style keys block
            if (jsonBlock.isMember(JsonKey(CommandsKey)) || jsonBlock.isMember(JsonKey(ActionKey)))
            {
                AddAction(*Command::FromJson(jsonBlock, warnings, origin, withKeybindings));

                // for non-nested non-iterable commands,
                // check if this is a legacy-style command block so we can inform the loader that fixups are needed
                if (jsonBlock.isMember(JsonKey(ActionKey)) && !jsonBlock.isMember(JsonKey(IterateOnKey)))
                {
                    if (jsonBlock.isMember(JsonKey(KeysKey)))
                    {
                        // there are keys in this command block - its the legacy style
                        _fixUpsAppliedDuringLoad = true;
                    }

                    if (origin == OriginTag::User && !jsonBlock.isMember(JsonKey(IDKey)))
                    {
                        // there's no ID in this command block - we will generate one for the user
                        // inform the loader that the ID needs to be written into the json
                        _fixUpsAppliedDuringLoad = true;
                    }
                }
            }
            else
            {
                // this is not a command block, so it is a keybinding block
                _AddKeyBindingHelper(jsonBlock, warnings);
            }
        }

        return warnings;
    }

    Json::Value ActionMap::ToJson() const
    {
        Json::Value actionList{ Json::ValueType::arrayValue };

        auto toJson = [&actionList](const Model::Command& cmd) {
            const auto cmdImpl{ winrt::get_self<implementation::Command>(cmd) };
            const auto& cmdJson{ cmdImpl->ToJson() };
            actionList.append(cmdJson);
        };

        for (const auto& [_, cmd] : _ActionMap)
        {
            toJson(cmd);
        }

        // Serialize all nested Command objects added in the current layer
        for (const auto& [_, cmd] : _NestedCommands)
        {
            toJson(cmd);
        }

        // Serialize all iterable Command objects added in the current layer
        for (const auto& cmd : _IterableCommands)
        {
            toJson(cmd);
        }

        return actionList;
    }

    Json::Value ActionMap::KeyBindingsToJson() const
    {
        Json::Value keybindingsList{ Json::ValueType::arrayValue };

        // Serialize all standard keybinding objects in the current layer
        for (const auto& [keys, cmdID] : _KeyMap)
        {
            Json::Value keyIDPair{ Json::ValueType::objectValue };
            JsonUtils::SetValueForKey(keyIDPair, KeysKey, keys);
            JsonUtils::SetValueForKey(keyIDPair, IDKey, cmdID);
            keybindingsList.append(keyIDPair);
        }

        return keybindingsList;
    }

    void ActionMap::_AddKeyBindingHelper(const Json::Value& json, std::vector<SettingsLoadWarnings>& warnings)
    {
        // There should always be a "keys" field
        // - If there is also an "id" field - we add the pair to our _KeyMap
        // - If there is no "id" field - this is an explicit unbinding, still add it to the _KeyMap,
        //   when this key chord is queried for we will know it is an explicit unbinding
        const auto keysJson{ json[JsonKey(KeysKey)] };
        if (keysJson.isArray() && keysJson.size() > 1)
        {
            warnings.push_back(SettingsLoadWarnings::TooManyKeysForChord);
            return;
        }

        Control::KeyChord keys{ nullptr };
        winrt::hstring idJson;
        if (!JsonUtils::GetValueForKey(json, KeysKey, keys))
        {
            return;
        }

        // if these keys are already bound to some command,
        // we need to update that command to erase these keys as we are about to overwrite them
        if (const auto foundCommand = _GetActionByKeyChordInternal(keys); foundCommand && *foundCommand)
        {
            const auto foundCommandImpl{ get_self<implementation::Command>(*foundCommand) };
            foundCommandImpl->EraseKey(keys);
        }

        // if the "id" field doesn't exist in the json, then idJson will be an empty string which is fine
        JsonUtils::GetValueForKey(json, IDKey, idJson);

        // any existing keybinding with the same keychord in this layer will get overwritten
        _KeyMap.insert_or_assign(keys, idJson);

        // make sure the command registers these keys
        if (!idJson.empty())
        {
            // TODO GH#17160
            // if the command with this id is only going to appear later during settings load
            // then this will return null, meaning that the command created later on will not register this keybinding
            // the keybinding will still work fine within the app, its just that the Command object itself won't know about this key mapping
            // we are going to move away from Command needing to know its key mappings in a followup, so this shouldn't matter for very long
            if (const auto cmd = _GetActionByID(idJson))
            {
                cmd.RegisterKey(keys);
            }
        }
        return;
    }
}
