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
static constexpr std::string_view ArgsKey{ "args" };

namespace winrt::TerminalApp::implementation
{
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
    // Return Value:
    // - the newly constructed Command object.
    winrt::com_ptr<Command> Command::FromJson(const Json::Value& json,
                                              std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
    {
        auto result = winrt::make_self<Command>();

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
                            result->_setName(GetLibraryResourceString(resourceKey));
                        }
                    }
                }
                CATCH_LOG();
            }
            else if (name.isString())
            {
                result->_setName(winrt::to_hstring(name.asString()));
            }
        }

        if (result->_Name.empty())
        {
            return nullptr;
        }

        // TODO GH#TODO: iconPath not implemented quite yet. Can't seem to get the binding quite right.
        if (const auto iconPathJson{ json[JsonKey(IconPathKey)] })
        {
            result->_setIconPath(winrt::to_hstring(iconPathJson.asString()));
        }

        if (const auto actionJson{ json[JsonKey(ActionKey)] })
        {
            auto actionAndArgs = ActionAndArgs::FromJson(actionJson, warnings);

            if (actionAndArgs)
            {
                result->_setAction(*actionAndArgs);
            }
            else
            {
                // TODO: { name: "foo", action: null } should _remove_ the "foo" command.
                return nullptr;
            }
        }

        return result;
    }

    std::vector<::TerminalApp::SettingsLoadWarnings> Command::LayerJson(std::map<winrt::hstring, winrt::TerminalApp::Command>& commands,
                                                                        const Json::Value& json)
    {
        std::vector<::TerminalApp::SettingsLoadWarnings> warnings;

        // TODO: Be smart about layering. Override commands with the same name.
        for (const auto& value : json)
        {
            if (value.isObject())
            {
                try
                {
                    auto result = Command::FromJson(value, warnings);
                    if (result)
                    {
                        commands.insert_or_assign(result->Name(), *result);
                    }
                    else
                    {
                        commands.erase(result->Name());
                    }
                }
                CATCH_LOG();
            }
        }
        return warnings;
    }
}
