// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Command.h"
#include "Command.g.cpp"

#include "Utils.h"
#include "ActionAndArgs.h"
#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconPathKey{ "iconPath" };
static constexpr std::string_view ActionKey{ "command" };
static constexpr std::string_view ArgsKey{ "args" };

static constexpr std::string_view IterateOnKey{ "iterateOn" };

static constexpr std::string_view IterateOnProfilesValue{ "profiles" };

static constexpr std::wstring_view ProfileName{ L"${profile.name}" };

namespace winrt::TerminalApp::implementation
{
    // Function Description:
    // - attempt to get the name of this command from the provided json object.
    //   * If the "name" property is a string, return that value.
    //   * If the "name" property is an object, attempt to lookup the string
    //     resource specified by the "key" property, to support localizable
    //     command names.
    // Arguments:
    // - json: The Json::Value representing the command object we should get the name for.
    // Return Value:
    // - the empty string if we couldn't find a name, otherwise the command's name.
    static winrt::hstring _nameFromJson(const Json::Value& json)
    {
        if (const auto name{ json[JsonKey(NameKey)] })
        {
            if (name.isObject())
            {
                try
                {
                    if (const auto keyJson{ name[JsonKey("key")] })
                    {
                        // Make sure the key is present before we try
                        // loading it. Otherwise we'll crash
                        const auto resourceKey = GetWstringFromJson(keyJson);
                        if (HasLibraryResourceWithName(resourceKey))
                        {
                            return GetLibraryResourceString(resourceKey);
                        }
                    }
                }
                CATCH_LOG();
            }
            else if (name.isString())
            {
                auto nameStr = name.asString();
                return winrt::to_hstring(nameStr);
            }
        }

        return L"";
    }

    // Method Description:
    // - Get the name for the command specified in `json`. If there is no "name"
    //   property in the provided json object, then instead generate a name for
    //   the provided ActionAndArgs.
    // Arguments:
    // - json: json for the command to generate a name for.
    // - actionAndArgs: An ActionAndArgs object to use to generate a name for,
    //   if the json object doesn't contain a "name".
    // Return Value:
    // - The "name" from the json, or the generated name from ActionAndArgs::GenerateName
    static winrt::hstring _nameFromJsonOrAction(const Json::Value& json,
                                                winrt::com_ptr<ActionAndArgs> actionAndArgs)
    {
        auto manualName = _nameFromJson(json);
        if (!manualName.empty())
        {
            return manualName;
        }
        if (!actionAndArgs)
        {
            return L"";
        }

        return actionAndArgs->GenerateName();
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
    winrt::com_ptr<Command> Command::FromJson(const Json::Value& json,
                                              std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
    {
        auto result = winrt::make_self<Command>();

        // bool parseActionLater = false;

        if (const auto iterateOnJson{ json[JsonKey(IterateOnKey)] })
        {
            auto s = iterateOnJson.asString();
            if (s == IterateOnProfilesValue)
            {
                result->_IterateOn = ExpandCommandType::Profiles;
                // parseActionLater = true;
            }
        }

        // if (!parseActionLater)
        // {
        // TODO GH#6644: iconPath not implemented quite yet. Can't seem to get
        // the binding quite right. Additionally, do we want it to be an image,
        // or a FontIcon? I've had difficulty binding either/or.

        if (const auto actionJson{ json[JsonKey(ActionKey)] })
        {
            auto actionAndArgs = ActionAndArgs::FromJson(actionJson, warnings);

            if (actionAndArgs)
            {
                result->_setAction(*actionAndArgs);
            }
            else
            {
                // Something like
                //      { name: "foo", action: "unbound" }
                // will _remove_ the "foo" command, by returning null here.
                return nullptr;
            }

            result->_setName(_nameFromJsonOrAction(json, actionAndArgs));
        }
        else
        {
            // { name: "foo", action: null } will land in this case, which
            // should also be used for unbinding.
            return nullptr;
        }
        // }
        // else
        // {
        // // Just use the current string as the name for now.
        // result->_setName(_nameFromJson(json));
        result->_originalJson = json;
        // }

        // TODO: an iterable command might not have a name set at all, and might
        // be relying on the command to be expanded, then have the name auto
        // generated. Make sure that those types of commands will still work.
        if (result->_Name.empty())
        {
            return nullptr;
        }

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
    std::vector<::TerminalApp::SettingsLoadWarnings> Command::LayerJson(std::unordered_map<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                                                        const Json::Value& json)
    {
        std::vector<::TerminalApp::SettingsLoadWarnings> warnings;

        for (const auto& value : json)
        {
            if (value.isObject())
            {
                try
                {
                    auto result = Command::FromJson(value, warnings);
                    if (result)
                    {
                        // Override commands with the same name
                        commands.insert_or_assign(result->Name(), *result);
                    }
                    else
                    {
                        // If there wasn't a parsed command, then try to get the
                        // name from the json blob. If that name currently
                        // exists in our list of commands, we should remove it.
                        const auto name = _nameFromJson(value);
                        if (!name.empty())
                        {
                            commands.erase(name);
                        }
                    }
                }
                CATCH_LOG();
            }
        }
        return warnings;
    }

    // Function Description:
    // - A helper to replace any occurences of `keyword` with `replaceWith` in `sourceString`
    winrt::hstring _replaceKeyword(const winrt::hstring& sourceString,
                                   const std::wstring_view keyword,
                                   const std::wstring_view replaceWith)
    {
        std::wstring result{ sourceString };
        size_t index = 0;
        while (true)
        {
            index = result.find(keyword, index);
            if (index == std::wstring::npos)
            {
                break;
            }
            result.replace(index, keyword.size(), replaceWith);
            index += replaceWith.size();
        }
        return winrt::hstring{ result };
    }

    // Function Description:
    // - Attempts to expand the given command into many commands, if the command
    //   has `"iterateOn": "profiles"` set.
    // - If it doesn't, this function will do
    //   nothing and return an empty vector.
    // - If it does, we're going to attempt to build a new set of commands using
    //   the given command as a prototype. We'll attempt to create a new command
    //   for each and every profile, to replace the original command.
    //   * For the new commands, we'll replace any instance of "${profile.name}"
    //     in the original json used to create this action with the name of the
    //     given profile.
    // - If we encounter any errors while re-parsing the json with the replaced
    //   name, we'll just return immediately.
    // - At the end, we'll return all the new commands we've build for the given command.
    // Arguments:
    // - expandable: the Command to potentially turn into more commands
    // - profiles: A list of all the profiles that this command should be expanded on.
    // - warnings: If there were any warnings during parsing, they'll be
    //   appended to this vector.
    // Return Value:
    // - and empty vector if the command wasn't expandable, otherwise a list of
    //   the newly-created commands.
    std::vector<winrt::TerminalApp::Command> Command::ExpandCommand(winrt::com_ptr<Command> expandable,
                                                                    const std::vector<::TerminalApp::Profile>& profiles,
                                                                    std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
    {
        std::vector<winrt::TerminalApp::Command> newCommands;

        if (expandable->_IterateOn == ExpandCommandType::None)
        {
            return newCommands;
        }

        std::string errs; // This string will receive any error text from failing to parse.
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        if (expandable->_IterateOn == ExpandCommandType::Profiles)
        {
            for (const auto& p : profiles)
            {
                // For each profile, create a new command. This command will have:
                // * the icon path and keychord text of the original command
                // * the Name will have any instances of "${profile.name}"
                //   replaced with the profile's name
                // * for the action, we'll take the original json, replace any
                //   instances of "${profile.name}" with the profile's name,
                //   then re-attempt to parse the action and args.
                //
                auto newCmd = winrt::make_self<Command>();
                // newCmd->_setIconPath(expandable->_IconPath);
                newCmd->_setKeyChordText(expandable->_KeyChordText);
                newCmd->_setName(_replaceKeyword(expandable->_Name,
                                                 ProfileName,
                                                 p.GetName()));

                // Replace all the keywords in the original json, and try and parse that
                auto oldJsonString = winrt::to_hstring(expandable->_originalJson.toStyledString());

                // TODO: We need to escape the profile name for JSON appropriately
                auto newJsonString = winrt::to_string(_replaceKeyword(oldJsonString,
                                                                      ProfileName,
                                                                      p.GetName()));

                Json::Value newJsonValue;
                const auto actualDataStart = newJsonString.data();
                const auto actualDataEnd = newJsonString.data() + newJsonString.size();
                if (!reader->parse(actualDataStart, actualDataEnd, &newJsonValue, &errs))
                {
                    warnings.push_back(::TerminalApp::SettingsLoadWarnings::FailedToParseCommandJson);
                    // If we encounter a re-parsing error, just stop processing the rest of the commands.
                    break;
                }

                // auto actionAndArgs = _getActionAndArgsFromJson(newJsonValue, warnings);

                if (const auto actionJson{ newJsonValue[JsonKey(ActionKey)] })
                {
                    auto actionAndArgs = ActionAndArgs::FromJson(actionJson, warnings);

                    if (actionAndArgs)
                    {
                        newCmd->_setAction(*actionAndArgs);
                    }
                    else
                    {
                        // Something like
                        //      { name: "foo", action: "unbound" }
                        // will _remove_ the "foo" command, by returning null here.
                        continue;
                    }

                    newCmd->_setName(_nameFromJsonOrAction(newJsonValue, actionAndArgs));
                }
                else
                {
                    // { name: "foo", action: null } will land in this case, which
                    // should also be used for unbinding.
                    continue;
                }

                // if (actionAndArgs && !newCmd->_Name.empty())
                if (!newCmd->_Name.empty())
                {
                    // newCmd->_setAction(*actionAndArgs);
                    newCommands.push_back(*newCmd);
                }
            }
        }

        return newCommands;
    }
}
