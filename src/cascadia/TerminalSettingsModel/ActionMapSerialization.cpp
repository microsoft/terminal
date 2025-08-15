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

            // if there are keys, extract them first
            Control::KeyChord keys{ nullptr };
            if (withKeybindings && jsonBlock.isMember(JsonKey(KeysKey)))
            {
                const auto keysJson{ jsonBlock[JsonKey(KeysKey)] };
                if (keysJson.isArray() && keysJson.size() > 1)
                {
                    warnings.push_back(SettingsLoadWarnings::TooManyKeysForChord);
                }
                else
                {
                    JsonUtils::GetValueForKey(jsonBlock, KeysKey, keys);
                }
            }

            // Now check if this is a command block
            if (jsonBlock.isMember(JsonKey(CommandsKey)) || jsonBlock.isMember(JsonKey(ActionKey)))
            {
                auto command = Command::FromJson(jsonBlock, warnings, origin);
                command->LogSettingChanges(_changeLog);
                AddAction(*command, keys);

                if (jsonBlock.isMember(JsonKey(KeysKey)))
                {
                    // there are keys in this command block meaning this is the legacy style -
                    // inform the loader that fixups are needed
                    _fixupsAppliedDuringLoad = true;
                }

                if (jsonBlock.isMember(JsonKey(ActionKey)) && !jsonBlock.isMember(JsonKey(IterateOnKey)) && origin == OriginTag::User && !jsonBlock.isMember(JsonKey(IDKey)))
                {
                    // for non-nested non-iterable commands,
                    // if there's no ID in the command block we will generate one for the user -
                    // inform the loader that the ID needs to be written into the json
                    _fixupsAppliedDuringLoad = true;
                }
            }
            else if (keys)
            {
                // this is not a command block, so it is a keybinding block

                // if the "id" field doesn't exist in the json, then idJson will be an empty string which is fine
                winrt::hstring idJson;
                JsonUtils::GetValueForKey(jsonBlock, IDKey, idJson);

                // any existing keybinding with the same keychord in this layer will get overwritten
                _KeyMap.insert_or_assign(keys, idJson);

                if (!_changeLog.contains(KeysKey.data()))
                {
                    // Log "keys" field, but only if it's one that isn't in userDefaults.json
                    static constexpr std::array<std::pair<std::wstring_view, std::string_view>, 3> userDefaultKbds{ { { L"Terminal.CopyToClipboard", "ctrl+c" },
                                                                                                                      { L"Terminal.PasteFromClipboard", "ctrl+v" },
                                                                                                                      { L"Terminal.DuplicatePaneAuto", "alt+shift+d" } } };
                    bool isUserDefaultKbd = false;
                    for (const auto& [id, kbd] : userDefaultKbds)
                    {
                        const auto keyJson{ jsonBlock.find(&*KeysKey.cbegin(), (&*KeysKey.cbegin()) + KeysKey.size()) };
                        if (idJson == id && keyJson->asString() == kbd)
                        {
                            isUserDefaultKbd = true;
                            break;
                        }
                    }
                    if (!isUserDefaultKbd)
                    {
                        _changeLog.emplace(KeysKey);
                    }
                }
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

    void ActionMap::LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const
    {
        for (const auto& setting : _changeLog)
        {
            changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), context, setting));
        }
    }
}
