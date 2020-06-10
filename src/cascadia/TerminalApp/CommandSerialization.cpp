// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Command.h"
#include "Utils.h"
#include "ActionAndArgs.h"
#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconPathKey{ "iconPath" };
static constexpr std::string_view ActionKey{ "action" };

static constexpr std::string_view IterateOnKey{ "iterateOn" };

static constexpr std::string_view IterateOnProfilesValue{ "profiles" };

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
    winrt::hstring _nameFromJson(const Json::Value& json)
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
                return winrt::to_hstring(name.asString());
            }
        }

        return L"";
    }

    winrt::com_ptr<ActionAndArgs> _getActionAndArgsFromJson(const Json::Value& json,
                                                            std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
    {
        if (const auto actionJson{ json[JsonKey(ActionKey)] })
        {
            return ActionAndArgs::FromJson(actionJson, warnings);
        }
        return nullptr;
    }

    // Method Description:
    // - Deserialize an Command from the `json` object. The json object should
    //   contain a "name" and "action", and optionally an "icon".
    //   * "name": string|object - the name of the command to display in the
    //     command palette. If this is an object, look for the "key" property,
    //     and try to load the string from our resources instead.
    //   * "action": string|object - A ShortcutAction, either as a name or as an
    //     ActionAndArgs serialization. See ActionAndArgs::FromJson for details.
    //     If this is null, we'll remove this command from the list of commands.
    //   * "iconPath": string? - the path to an icon to use with this command entry
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

        result->_setName(_nameFromJson(json));

        if (result->_Name.empty())
        {
            return nullptr;
        }

        // TODO GH#TODO: iconPath not implemented quite yet. Can't seem to get the binding quite right.
        if (const auto iconPathJson{ json[JsonKey(IconPathKey)] })
        {
            result->_setIconPath(winrt::to_hstring(iconPathJson.asString()));
        }
        bool parseActionLater = false;

        if (const auto iterateOnJson{ json[JsonKey(IterateOnKey)] })
        {
            auto s = iterateOnJson.asString();
            if (s == IterateOnProfilesValue)
            {
                result->_IterateOn = ExpandCommandType::Profiles;
                parseActionLater = true;
            }
        }

        if (!parseActionLater)
        {
            auto actionAndArgs = _getActionAndArgsFromJson(json, warnings);

            if (actionAndArgs)
            {
                result->_setAction(*actionAndArgs);
            }
            else
            {
                // Something like
                //      { name: "foo", action: null }
                // will _remove_ the "foo" command, by returning null here.
                return nullptr;
            }
        }
        else
        {
            result->_originalJson = json;
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
    std::vector<::TerminalApp::SettingsLoadWarnings> Command::LayerJson(std::map<winrt::hstring, winrt::TerminalApp::Command>& commands,
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
                        const auto name = _nameFromJson(json);
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

    std::vector<winrt::TerminalApp::Command> Command::ExpandCommand(winrt::com_ptr<Command> expandable,
                                                                    const std::vector<::TerminalApp::Profile>& profiles)
    {
        std::vector<::TerminalApp::SettingsLoadWarnings> warnings;
        std::vector<winrt::TerminalApp::Command> newCommands;

        if (expandable->_IterateOn == ExpandCommandType::Profiles)
        {
            // commands.erase(expandable->_Name);

            for (const auto& p : profiles)
            {
                auto newCmd = winrt::make_self<Command>();
                newCmd->_setIconPath(expandable->_IconPath);
                newCmd->_setKeyChordText(expandable->_KeyChordText);
                newCmd->_setName(_replaceKeyword(expandable->_Name,
                                                 L"${profile.name}",
                                                 p.GetName()));

                // Replace all the keywords in the original json, and try and parse that
                auto oldJsonString = winrt::to_hstring(expandable->_originalJson.toStyledString());
                auto newJsonString = winrt::to_string(_replaceKeyword(oldJsonString,
                                                                      L"${profile.name}",
                                                                      p.GetName()));

                Json::Value newJsonValue; // = Json::Value{newJsonString};

                std::string errs; // This string will receive any error text from failing to parse.
                std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };
                auto actualDataStart = newJsonString.data();
                const auto actualDataEnd = newJsonString.data() + newJsonString.size();
                if (!reader->parse(actualDataStart, actualDataEnd, &newJsonValue, &errs))
                {
                    // This will be caught by App::_TryLoadSettings, who will display
                    // the text to the user.
                    throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
                }

                auto actionAndArgs = _getActionAndArgsFromJson(newJsonValue, warnings);

                // newCmd->_setAction(expandable._Action);
                if (actionAndArgs && !newCmd->_Name.empty())
                {
                    newCmd->_setAction(*actionAndArgs);
                    newCommands.push_back(*newCmd);
                }
            }
        }

        return newCommands;
    }
}
