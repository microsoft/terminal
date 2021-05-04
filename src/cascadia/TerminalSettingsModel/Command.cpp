// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Command.h"
#include "Command.g.cpp"

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

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view ActionKey{ "command" };
static constexpr std::string_view ArgsKey{ "args" };
static constexpr std::string_view IterateOnKey{ "iterateOn" };
static constexpr std::string_view CommandsKey{ "commands" };
static constexpr std::string_view KeysKey{ "keys" };

static constexpr std::string_view ProfileNameToken{ "${profile.name}" };
static constexpr std::string_view ProfileIconToken{ "${profile.icon}" };
static constexpr std::string_view SchemeNameToken{ "${scheme.name}" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    Command::Command()
    {
    }

    com_ptr<Command> Command::Copy() const
    {
        auto command{ winrt::make_self<Command>() };
        command->_name = _name;
        command->_ActionAndArgs = *get_self<implementation::ActionAndArgs>(_ActionAndArgs)->Copy();
        command->_keyMappings = _keyMappings;
        command->_iconPath = _iconPath;
        command->_IterateOn = _IterateOn;

        command->_originalJson = _originalJson;
        if (HasNestedCommands())
        {
            command->_subcommands = winrt::single_threaded_map<winrt::hstring, Model::Command>();
            for (auto kv : NestedCommands())
            {
                const auto subCmd{ winrt::get_self<Command>(kv.Value()) };
                command->_subcommands.Insert(kv.Key(), *subCmd->Copy());
            }
        }
        return command;
    }

    IMapView<winrt::hstring, Model::Command> Command::NestedCommands() const
    {
        return _subcommands ? _subcommands.GetView() : nullptr;
    }

    // Function Description:
    // - reports if the current command has nested commands
    // - This CANNOT detect { "name": "foo", "commands": null }
    bool Command::HasNestedCommands() const
    {
        return _subcommands ? _subcommands.Size() > 0 : false;
    }

    // Function Description:
    // - reports if the current command IS a nested command
    // - This CAN be used to detect cases like { "name": "foo", "commands": null }
    bool Command::IsNestedCommand() const noexcept
    {
        return _nestedCommand;
    }

    bool Command::HasName() const noexcept
    {
        return _name.has_value();
    }

    hstring Command::Name() const noexcept
    {
        if (_name.has_value())
        {
            // name was explicitly set, return that value.
            return hstring{ _name.value() };
        }
        else if (_ActionAndArgs)
        {
            // generate a name from our action
            return get_self<implementation::ActionAndArgs>(_ActionAndArgs)->GenerateName();
        }
        else
        {
            // we have no name
            return {};
        }
    }

    void Command::Name(const hstring& value)
    {
        if (!_name.has_value() || _name.value() != value)
        {
            _name = value;
        }
    }

    std::vector<Control::KeyChord> Command::KeyMappings() const noexcept
    {
        return _keyMappings;
    }

    // Function Description:
    // - Add the key chord to the command's list of key mappings.
    // - If the key chord was already registered, move it to the back
    //   of the line, and dispatch a notification that Command::Keys changed.
    // Arguments:
    // - keys: the new key chord that we are registering this command to
    // Return Value:
    // - <none>
    void Command::RegisterKey(const Control::KeyChord& keys)
    {
        if (!keys)
        {
            return;
        }

        // Remove the KeyChord and add it to the back of the line.
        // This makes it so that the main key chord associated with this
        // command is updated.
        EraseKey(keys);
        _keyMappings.push_back(keys);
    }

    // Function Description:
    // - Remove the key chord from the command's list of key mappings.
    // Arguments:
    // - keys: the key chord that we are unregistering
    // Return Value:
    // - <none>
    void Command::EraseKey(const Control::KeyChord& keys)
    {
        _keyMappings.erase(std::remove_if(_keyMappings.begin(), _keyMappings.end(), [&keys](const Control::KeyChord& iterKey) {
                               return keys.Modifiers() == iterKey.Modifiers() && keys.Vkey() == iterKey.Vkey();
                           }),
                           _keyMappings.end());
    }

    // Function Description:
    // - Keys is the Command's identifying KeyChord. The command may have multiple keys associated
    //   with it, but we'll only ever display the most recently added one externally. To do this,
    //   _keyMappings stores all of the associated key chords, but ensures that the last entry
    //   is the most recently added one.
    // Arguments:
    // - <none>
    // Return Value:
    // - the primary key chord associated with this Command
    Control::KeyChord Command::Keys() const noexcept
    {
        if (_keyMappings.empty())
        {
            return nullptr;
        }
        return _keyMappings.back();
    }

    hstring Command::KeyChordText() const noexcept
    {
        return KeyChordSerialization::ToString(Keys());
    }

    hstring Command::IconPath() const noexcept
    {
        if (_iconPath.has_value())
        {
            return hstring{ *_iconPath };
        }
        return {};
    }

    void Command::IconPath(const hstring& val)
    {
        if (!_iconPath.has_value() || _iconPath.value() != val)
        {
            _iconPath = val;
        }
    }

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
    static std::optional<std::wstring> _nameFromJson(const Json::Value& json)
    {
        if (const auto name{ json[JsonKey(NameKey)] })
        {
            if (name.isObject())
            {
                if (const auto resourceKey{ JsonUtils::GetValueForKey<std::optional<std::wstring>>(name, "key") })
                {
                    if (HasLibraryResourceWithName(*resourceKey))
                    {
                        return std::wstring{ GetLibraryResourceString(*resourceKey) };
                    }
                }
            }
            else if (name.isString())
            {
                return JsonUtils::GetValue<std::wstring>(name);
            }
        }
        else if (json.isMember(JsonKey(NameKey)))
        {
            // { "name": null, "command": "copy" } will land in this case, which
            // should also be used for unbinding.
            return std::wstring{};
        }

        return std::nullopt;
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
                                              std::vector<SettingsLoadWarnings>& warnings)
    {
        auto result = winrt::make_self<Command>();

        bool nested = false;
        JsonUtils::GetValueForKey(json, IterateOnKey, result->_IterateOn);

        // For iterable commands, we'll make another pass at parsing them once
        // the json is patched. So ignore parsing sub-commands for now. Commands
        // will only be marked iterable on the first pass.
        if (const auto nestedCommandsJson{ json[JsonKey(CommandsKey)] })
        {
            // Initialize our list of subcommands.
            result->_subcommands = winrt::single_threaded_map<winrt::hstring, Model::Command>();
            result->_nestedCommand = true;
            auto nestedWarnings = Command::LayerJson(result->_subcommands, nestedCommandsJson);
            // It's possible that the nested commands have some warnings
            warnings.insert(warnings.end(), nestedWarnings.begin(), nestedWarnings.end());

            if (result->_subcommands.Size() == 0)
            {
                warnings.push_back(SettingsLoadWarnings::FailedToParseSubCommands);
                result->_ActionAndArgs = make<implementation::ActionAndArgs>();
            }

            nested = true;
        }
        else if (json.isMember(JsonKey(CommandsKey)))
        {
            // { "name": "foo", "commands": null } will land in this case, which
            // should also be used for unbinding.

            // create an "invalid" ActionAndArgs
            result->_ActionAndArgs = make<implementation::ActionAndArgs>();
            result->_nestedCommand = true;
        }

        JsonUtils::GetValueForKey(json, IconKey, result->_iconPath);

        // If we're a nested command, we can ignore the current action.
        if (!nested)
        {
            if (const auto actionJson{ json[JsonKey(ActionKey)] })
            {
                result->_ActionAndArgs = *ActionAndArgs::FromJson(actionJson, warnings);
            }
            else
            {
                // { name: "foo", action: null } will land in this case, which
                // should also be used for unbinding.

                // create an "invalid" ActionAndArgs
                result->_ActionAndArgs = make<implementation::ActionAndArgs>();
            }

            // GH#4239 - If the user provided more than one key
            // chord to a "keys" array, warn the user here.
            // TODO: GH#1334 - remove this check.
            const auto keysJson{ json[JsonKey(KeysKey)] };
            if (keysJson.isArray() && keysJson.size() > 1)
            {
                warnings.push_back(SettingsLoadWarnings::TooManyKeysForChord);
            }
            else
            {
                Control::KeyChord keys{ nullptr };
                if (JsonUtils::GetValueForKey(json, KeysKey, keys))
                {
                    result->RegisterKey(keys);
                }
            }
        }

        // If an iterable command doesn't have a name set, we'll still just
        // try and generate a fake name for the command give the string we
        // currently have. It'll probably generate something like "New tab,
        // profile: ${profile.name}". This string will only be temporarily
        // used internally, so there's no problem.
        result->_name = _nameFromJson(json);

        // Stash the original json value in this object. If the command is
        // iterable, we'll need to re-parse it later, once we know what all the
        // values we can iterate on are.
        result->_originalJson = json;

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
    std::vector<SettingsLoadWarnings> Command::LayerJson(IMap<winrt::hstring, Model::Command>& commands,
                                                         const Json::Value& json)
    {
        std::vector<SettingsLoadWarnings> warnings;

        for (const auto& value : json)
        {
            if (value.isObject())
            {
                try
                {
                    const auto result = Command::FromJson(value, warnings);
                    if (result->ActionAndArgs().Action() == ShortcutAction::Invalid && !result->HasNestedCommands())
                    {
                        // If there wasn't a parsed command, then try to get the
                        // name from the json blob. If that name currently
                        // exists in our list of commands, we should remove it.
                        const auto name = _nameFromJson(value);
                        if (name.has_value() && !name->empty())
                        {
                            commands.Remove(*name);
                        }
                    }
                    else
                    {
                        // Override commands with the same name
                        commands.Insert(result->Name(), *result);
                    }
                }
                CATCH_LOG();
            }
        }
        return warnings;
    }

    // Function Description:
    // - Helper to escape a string as a json string. This function will also
    //   trim off the leading and trailing double-quotes, so the output string
    //   can be inserted directly into another json blob.
    // Arguments:
    // - input: the string to JSON escape.
    // Return Value:
    // - the input string escaped properly to be inserted into another json blob.
    std::string _escapeForJson(const std::string& input)
    {
        Json::Value inJson{ input };
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        std::string out{ Json::writeString(builder, inJson) };
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
    void Command::ExpandCommands(IMap<winrt::hstring, Model::Command> commands,
                                 IVectorView<Model::Profile> profiles,
                                 IVectorView<Model::ColorScheme> schemes,
                                 IVector<SettingsLoadWarnings> warnings)
    {
        std::vector<winrt::hstring> commandsToRemove;
        std::vector<Model::Command> commandsToAdd;

        // First, collect up all the commands that need replacing.
        for (const auto& nameAndCmd : commands)
        {
            auto cmd{ get_self<implementation::Command>(nameAndCmd.Value()) };

            auto newCommands = _expandCommand(cmd, profiles, schemes, warnings);
            if (newCommands.size() > 0)
            {
                commandsToRemove.push_back(nameAndCmd.Key());
                commandsToAdd.insert(commandsToAdd.end(), newCommands.begin(), newCommands.end());
            }
        }

        // Second, remove all the commands that need to be removed.
        for (auto& name : commandsToRemove)
        {
            commands.Remove(name);
        }

        // Finally, add all the new commands.
        for (auto& cmd : commandsToAdd)
        {
            commands.Insert(cmd.Name(), cmd);
        }
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
    std::vector<Model::Command> Command::_expandCommand(Command* const expandable,
                                                        IVectorView<Model::Profile> profiles,
                                                        IVectorView<Model::ColorScheme> schemes,
                                                        IVector<SettingsLoadWarnings>& warnings)
    {
        std::vector<Model::Command> newCommands;

        if (expandable->HasNestedCommands())
        {
            ExpandCommands(expandable->_subcommands, profiles, schemes, warnings);
        }

        if (expandable->_IterateOn == ExpandCommandType::None)
        {
            return newCommands;
        }

        std::string errs; // This string will receive any error text from failing to parse.
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        // First, get a string for the original Json::Value
        auto oldJsonString = expandable->_originalJson.toStyledString();

        auto reParseJson = [&](const auto& newJsonString) -> bool {
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
            if (auto newCmd{ Command::FromJson(newJsonValue, newWarnings) })
            {
                newCommands.push_back(*newCmd);
            }
            std::for_each(newWarnings.begin(), newWarnings.end(), [warnings](auto& warn) { warnings.Append(warn); });
            return true;
        };

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

                // Replace all the keywords in the original json, and try and parse that

                // - Escape the profile name for JSON appropriately
                auto escapedProfileName = _escapeForJson(til::u16u8(p.Name()));
                auto escapedProfileIcon = _escapeForJson(til::u16u8(p.Icon()));
                auto newJsonString = til::replace_needle_in_haystack(oldJsonString,
                                                                     ProfileNameToken,
                                                                     escapedProfileName);
                til::replace_needle_in_haystack_inplace(newJsonString,
                                                        ProfileIconToken,
                                                        escapedProfileIcon);

                // If we encounter a re-parsing error, just stop processing the rest of the commands.
                if (!reParseJson(newJsonString))
                {
                    break;
                }
            }
        }
        else if (expandable->_IterateOn == ExpandCommandType::ColorSchemes)
        {
            for (const auto& s : schemes)
            {
                // For each scheme, create a new command. We'll take the
                // original json, replace any instances of "${scheme.name}" with
                // the scheme's name, then re-attempt to parse the action and
                // args.

                // - Escape the profile name for JSON appropriately
                auto escapedSchemeName = _escapeForJson(til::u16u8(s.Name()));
                auto newJsonString = til::replace_needle_in_haystack(oldJsonString,
                                                                     SchemeNameToken,
                                                                     escapedSchemeName);

                // If we encounter a re-parsing error, just stop processing the rest of the commands.
                if (!reParseJson(newJsonString))
                {
                    break;
                }
            }
        }

        return newCommands;
    }
}
