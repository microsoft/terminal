// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Trigger.h"
#include "Trigger.g.cpp"

#include "ActionAndArgs.h"
#include "KeyChordSerialization.h"
#include <LibraryResources.h>
#include "TerminalSettingsSerializationHelpers.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;
using namespace ::Microsoft::Terminal::Settings::Model;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

static constexpr std::string_view ActionKey{ "command" };
static constexpr std::string_view MatchKey{ "match" };

static constexpr std::string_view sure{ "%d" };
static constexpr std::string_view MatchToken{ "${match[{}]}" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    com_ptr<Trigger> Trigger::Copy() const
    {
        auto trigger{ winrt::make_self<Trigger>() };

        trigger->_ActionAndArgs = *get_self<implementation::ActionAndArgs>(_ActionAndArgs)->Copy();
        trigger->_originalActionJson = _originalActionJson;
        trigger->_Type = _Type;
        trigger->_Match = _Match;

        return trigger;
    }

    // Method Description:
    // - Deserialize a Command from the `json` object. The json object should
    //   contain a "name" and "action", and optionally an "icon".
    //   * "name": string|object - the name of the command to display in the
    //     command palette. If this is an object, look for the "key" property,
    //     and try to load the string from our resources instead.
    //   * "action": string|object - A ShortcutAction, either as a name or as an
    //     ActionAndArgs serialization. See ActionAndArgs::FromJson for details.
    //     If this is null, we'll remove this command from the list of commands.
    // Arguments:
    // - json: the Json::Value to deserialize into a Command
    // - warnings: If there were any warnings during parsing, they'll be
    //   appended to this vector.
    // Return Value:
    // - the newly constructed Command object.
    winrt::com_ptr<Trigger> Trigger::FromJson(const Json::Value& json)
    {
        auto result = winrt::make_self<Trigger>();

        JsonUtils::GetValueForKey(json, MatchKey, result->_Match);

        // If we're a nested command, we can ignore the current action.
        result->_originalActionJson = json[JsonKey(ActionKey)];

        return result;
    }

    // Function Description:
    // - Attempt to parse all the json objects in `json` into new Command
    //   objects, and add them to the map of commands.
    // - If any parsed command has
    //   the same Name as an existing command in commands, the new one will
    //   layer on top of the existing one.
    // Arguments:
    // - commands: a map of Name->Command which new commands should be layered upon.
    // - json: A Json::Value containing an array of serialized commands
    // Return Value:
    // - A vector containing any warnings detected while parsing
    std::vector<SettingsLoadWarnings> Trigger::LayerJson(Windows::Foundation::Collections::IVector<Model::Trigger>& triggers,
                                                         const Json::Value& json)
    {
        std::vector<SettingsLoadWarnings> warnings;

        for (const auto& value : json)
        {
            if (value.isObject())
            {
                try
                {
                    const auto result = Trigger::FromJson(value);
                    // if (result->ActionAndArgs().Action() == ShortcutAction::Invalid && !result->HasNestedCommands())
                    // {
                    //     // If there wasn't a parsed command, then try to get the
                    //     // name from the json blob. If that name currently
                    //     // exists in our list of commands, we should remove it.
                    //     const auto name = _nameFromJson(value);
                    //     if (name.has_value() && !name->empty())
                    //     {
                    //         commands.Remove(*name);
                    //     }
                    // }
                    // else
                    // {
                    //     // Override commands with the same name
                    //     commands.Insert(result->Name(), *result);
                    // }
                    triggers.Append(*result);
                }
                CATCH_LOG();
            }
        }
        return warnings;
    }

    // Function Description:
    // - Serialize the Command into an array of json actions
    // Arguments:
    // - <none>
    // Return Value:
    // - an array of serialized actions
    Json::Value Trigger::ToJson() const
    {
        //TODO!

        // Json::Value cmdList{ Json::ValueType::arrayValue };

        // if (_nestedCommand || _IterateOn != ExpandCommandType::None)
        // {
        //     // handle special commands
        //     // For these, we can trust _originalJson to be correct.
        //     // In fact, we _need_ to use it here because we don't actually deserialize `iterateOn`
        //     //   until we expand the command.
        //     cmdList.append(_originalJson);
        // }
        // else if (_keyMappings.empty())
        // {
        //     // only write out one command
        //     Json::Value cmdJson{ Json::ValueType::objectValue };
        //     JsonUtils::SetValueForKey(cmdJson, IconKey, _iconPath);
        //     JsonUtils::SetValueForKey(cmdJson, NameKey, _name);

        //     if (_ActionAndArgs)
        //     {
        //         cmdJson[JsonKey(ActionKey)] = ActionAndArgs::ToJson(_ActionAndArgs);
        //     }

        //     cmdList.append(cmdJson);
        // }
        // else
        // {
        //     // we'll write out one command per key mapping
        //     for (auto keys{ _keyMappings.begin() }; keys != _keyMappings.end(); ++keys)
        //     {
        //         Json::Value cmdJson{ Json::ValueType::objectValue };

        //         if (keys == _keyMappings.begin())
        //         {
        //             // First iteration also writes icon and name
        //             JsonUtils::SetValueForKey(cmdJson, IconKey, _iconPath);
        //             JsonUtils::SetValueForKey(cmdJson, NameKey, _name);
        //         }

        //         if (_ActionAndArgs)
        //         {
        //             cmdJson[JsonKey(ActionKey)] = ActionAndArgs::ToJson(_ActionAndArgs);
        //         }

        //         JsonUtils::SetValueForKey(cmdJson, KeysKey, *keys);
        //         cmdList.append(cmdJson);
        //     }
        // }

        // return cmdList;
        return Json::Value{};
    }

    // Function Description:
    // - Helper to escape a string as a json string. This function will also
    //   trim off the leading and trailing double-quotes, so the output string
    //   can be inserted directly into another json blob.
    // Arguments:
    // - input: the string to JSON escape.
    // Return Value:
    // - the input string escaped properly to be inserted into another json blob.
    static std::string _escapeForJson(const std::string& input)
    {
        Json::Value inJson{ input };
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        auto out{ Json::writeString(builder, inJson) };
        if (out.size() >= 2)
        {
            // trim off the leading/trailing '"'s
            auto ss{ out.substr(1, out.size() - 2) };
            return ss;
        }
        return out;
    }

    // Method Description:
    // - Iterate over all the provided commands, and recursively expand any
    //   commands with `iterateOn` set. If we successfully generated expanded
    //   commands for them, then we'll remove the original command, and add all
    //   the newly generated commands.
    // - For more specific implementation details, see _expandCommand.
    // Arguments:
    // - commands: a map of commands to expand. Newly created commands will be
    //   inserted into the map to replace the expandable commands.
    // - profiles: A list of all the profiles that this command should be expanded on.
    // - warnings: If there were any warnings during parsing, they'll be
    //   appended to this vector.
    // Return Value:
    // - <none>
    Model::ActionAndArgs Trigger::EvaluateMatch(Windows::Foundation::Collections::IVectorView<winrt::hstring> matches,
                                                Windows::Foundation::Collections::IVector<SettingsLoadWarnings> warnings)
    {
        std::string errs; // This string will receive any error text from failing to parse.
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        // First, get a string for the original Json::Value
        auto oldJsonString = _originalActionJson.toStyledString();

        auto reParseJson = [&](const auto& newJsonString) -> Model::ActionAndArgs {
            // - Now, re-parse the modified value.
            Json::Value newJsonValue;
            const auto actualDataStart = newJsonString.data();
            const auto actualDataEnd = newJsonString.data() + newJsonString.size();
            if (!reader->parse(actualDataStart, actualDataEnd, &newJsonValue, &errs))
            {
                warnings.Append(SettingsLoadWarnings::FailedToParseCommandJson);
                // If we encounter a re-parsing error, just stop processing the rest of the commands.
                return false;
            }

            // Pass the new json back though FromJson, to get the new expanded value.
            std::vector<SettingsLoadWarnings> newWarnings;
            auto result = ActionAndArgs::FromJson(newJsonValue, newWarnings);
            std::for_each(newWarnings.begin(), newWarnings.end(), [warnings](auto& warn) { warnings.Append(warn); });
            return *result;
        };

        auto newJsonString = oldJsonString;
        for (uint32_t i = 0u; i < matches.Size(); i++)
        {
            auto newNeedle = fmt::format(MatchToken, i);
            auto escapedMatch = _escapeForJson(til::u16u8(matches.GetAt(i)));

            til::replace_needle_in_haystack(newJsonString,
                                            newNeedle,
                                            escapedMatch);
        }

        return reParseJson(newJsonString);
    }

}
