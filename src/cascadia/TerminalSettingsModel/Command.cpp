// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Command.h"
#include "Command.g.cpp"

#include <LibraryResources.h>
#include <til/replace.h>

#include "KeyChordSerialization.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;
using namespace ::Microsoft::Terminal::Settings::Model;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

static constexpr std::string_view ProfileNameToken{ "${profile.name}" };
static constexpr std::string_view ProfileIconToken{ "${profile.icon}" };
static constexpr std::string_view SchemeNameToken{ "${scheme.name}" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    Command::Command() = default;

    com_ptr<Command> Command::Copy() const
    {
        auto command{ winrt::make_self<Command>() };
        command->_name = _name;
        command->_Origin = _Origin;
        command->_ID = _ID;
        command->_ActionAndArgs = *get_self<implementation::ActionAndArgs>(_ActionAndArgs)->Copy();
        command->_iconPath = _iconPath;
        command->_IterateOn = _IterateOn;
        command->_Description = _Description;

        command->_originalJson = _originalJson;
        command->_nestedCommand = _nestedCommand;
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

    void Command::NestedCommands(const Windows::Foundation::Collections::IVectorView<Model::Command>& nested)
    {
        _subcommands = winrt::single_threaded_map<winrt::hstring, Model::Command>();

        for (const auto& n : nested)
        {
            _subcommands.Insert(n.Name(), n);
        }
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

    hstring Command::ID() const noexcept
    {
        return hstring{ _ID };
    }

    void Command::GenerateID()
    {
        if (_ActionAndArgs)
        {
            auto actionAndArgsImpl{ winrt::get_self<implementation::ActionAndArgs>(_ActionAndArgs) };
            if (const auto generatedID = actionAndArgsImpl->GenerateID(); !generatedID.empty())
            {
                _ID = generatedID;
                _IDWasGenerated = true;
            }
        }
    }

    bool Command::IDWasGenerated()
    {
        return _IDWasGenerated;
    }

    void Command::Name(const hstring& value)
    {
        if (!_name.has_value() || _name.value() != value)
        {
            _name = value;
        }
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
                                              std::vector<SettingsLoadWarnings>& warnings,
                                              const OriginTag origin)
    {
        auto result = winrt::make_self<Command>();
        result->_Origin = origin;
        JsonUtils::GetValueForKey(json, IDKey, result->_ID);

        auto nested = false;
        JsonUtils::GetValueForKey(json, IterateOnKey, result->_IterateOn);
        JsonUtils::GetValueForKey(json, DescriptionKey, result->_Description);

        // For iterable commands, we'll make another pass at parsing them once
        // the json is patched. So ignore parsing sub-commands for now. Commands
        // will only be marked iterable on the first pass.
        if (const auto nestedCommandsJson{ json[JsonKey(CommandsKey)] })
        {
            // Initialize our list of subcommands.
            result->_subcommands = winrt::single_threaded_map<winrt::hstring, Model::Command>();
            result->_nestedCommand = true;
            auto nestedWarnings = Command::LayerJson(result->_subcommands, nestedCommandsJson, origin);
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

    // This is substantially simpler than the normal FromJson. We just want to take something that looks like:
    // {
    //     "input": "bx",
    //     "name": "Build project",
    //     "description": "Build the project in the CWD"
    // },
    //
    // and turn it into a sendInput action. No need to figure out what kind of
    // action parser, or deal with nesting, or iterable commands or anything.
    winrt::com_ptr<Command> Command::FromSnippetJson(const Json::Value& json)
    {
        auto result = winrt::make_self<Command>();
        result->_Origin = OriginTag::Generated;

        JsonUtils::GetValueForKey(json, IDKey, result->_ID);
        JsonUtils::GetValueForKey(json, DescriptionKey, result->_Description);
        JsonUtils::GetValueForKey(json, IconKey, result->_iconPath);
        result->_name = _nameFromJson(json);

        const auto action{ ShortcutAction::SendInput };
        IActionArgs args{ nullptr };
        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> parseWarnings;
        std::tie(args, parseWarnings) = SendInputArgs::FromJson(json);
        result->_ActionAndArgs = winrt::make<implementation::ActionAndArgs>(action, args);

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
                                                         const Json::Value& json,
                                                         const OriginTag origin)
    {
        std::vector<SettingsLoadWarnings> warnings;

        for (const auto& value : json)
        {
            if (value.isObject())
            {
                try
                {
                    const auto result = Command::FromJson(value, warnings, origin);
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
    // - Serialize the Command into a json value
    // Arguments:
    // - <none>
    // Return Value:
    // - a serialized command
    Json::Value Command::ToJson() const
    {
        Json::Value cmdJson{ Json::ValueType::objectValue };

        if (_nestedCommand || _IterateOn != ExpandCommandType::None)
        {
            // handle special commands
            // For these, we can trust _originalJson to be correct.
            // In fact, we _need_ to use it here because we don't actually deserialize `iterateOn`
            //   until we expand the command.
            cmdJson = _originalJson;
        }
        else
        {
            JsonUtils::SetValueForKey(cmdJson, IconKey, _iconPath);
            JsonUtils::SetValueForKey(cmdJson, NameKey, _name);
            if (!_Description.empty())
            {
                JsonUtils::SetValueForKey(cmdJson, DescriptionKey, _Description);
            }
            if (!_ID.empty())
            {
                JsonUtils::SetValueForKey(cmdJson, IDKey, _ID);
            }

            if (_ActionAndArgs)
            {
                cmdJson[JsonKey(ActionKey)] = ActionAndArgs::ToJson(_ActionAndArgs);
            }
        }

        return cmdJson;
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
    void Command::ExpandCommands(IMap<winrt::hstring, Model::Command>& commands,
                                 IVectorView<Model::Profile> profiles,
                                 IVectorView<Model::ColorScheme> schemes)
    {
        std::vector<winrt::hstring> commandsToRemove;
        std::vector<Model::Command> commandsToAdd;

        // First, collect up all the commands that need replacing.
        for (const auto& [name, command] : commands)
        {
            auto cmd{ get_self<implementation::Command>(command) };

            auto newCommands = _expandCommand(cmd, profiles, schemes);
            if (newCommands.size() > 0)
            {
                commandsToRemove.push_back(name);
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
    // Return Value:
    // - and empty vector if the command wasn't expandable, otherwise a list of
    //   the newly-created commands.
    std::vector<Model::Command> Command::_expandCommand(Command* const expandable,
                                                        IVectorView<Model::Profile> profiles,
                                                        IVectorView<Model::ColorScheme> schemes)
    {
        std::vector<Model::Command> newCommands;

        if (expandable->HasNestedCommands())
        {
            ExpandCommands(expandable->_subcommands, profiles, schemes);
        }

        if (expandable->_IterateOn == ExpandCommandType::None)
        {
            return newCommands;
        }

        std::string errs; // This string will receive any error text from failing to parse.
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };

        // First, get a string for the original Json::Value
        auto oldJsonString = expandable->_originalJson.toStyledString();

        auto reParseJson = [&](const auto& newJsonString) -> bool {
            // - Now, re-parse the modified value.
            Json::Value newJsonValue;
            const auto actualDataStart = newJsonString.data();
            const auto actualDataEnd = newJsonString.data() + newJsonString.size();
            if (!reader->parse(actualDataStart, actualDataEnd, &newJsonValue, &errs))
            {
                // If we encounter a re-parsing error, just stop processing the rest of the commands.
                return false;
            }

            // Pass the new json back though FromJson, to get the new expanded value.
            // FromJson requires that we pass in a vector to hang on to the
            // warnings, but ultimately, we don't care about warnings during
            // expansion.
            std::vector<SettingsLoadWarnings> unused;
            if (auto newCmd{ Command::FromJson(newJsonValue, unused, expandable->_Origin) })
            {
                newCommands.push_back(*newCmd);
            }
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
                auto escapedProfileIcon = _escapeForJson(til::u16u8(p.EvaluatedIcon()));
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

    winrt::Windows::Foundation::Collections::IVector<Model::Command> Command::ParsePowerShellMenuComplete(winrt::hstring json, int32_t replaceLength)
    {
        if (json.empty())
        {
            return nullptr;
        }
        auto data = winrt::to_string(json);

        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
        Json::Value root;
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
        }

        std::vector<Model::Command> result;

        const auto parseElement = [&](const auto& element) {
            winrt::hstring completionText;
            winrt::hstring listText;
            winrt::hstring tooltipText;
            JsonUtils::GetValueForKey(element, "CompletionText", completionText);
            JsonUtils::GetValueForKey(element, "ListItemText", listText);
            JsonUtils::GetValueForKey(element, "ToolTip", tooltipText);

            auto args = winrt::make_self<SendInputArgs>(
                winrt::hstring{ fmt::format(FMT_COMPILE(L"{:\x7f^{}}{}"),
                                            L"",
                                            replaceLength,
                                            static_cast<std::wstring_view>(completionText)) });

            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, *args };

            auto c = winrt::make_self<Command>();
            c->_name = listText;
            c->_Description = tooltipText;
            c->_ActionAndArgs = actionAndArgs;
            // Try to assign a sensible icon based on the result type. These are
            // roughly chosen to align with the icons in
            // https://github.com/PowerShell/PowerShellEditorServices/pull/1738
            // as best as possible.
            if (const auto resultType{ JsonUtils::GetValueForKey<int>(element, "ResultType") })
            {
                // PowerShell completion result -> Segoe Fluent icon value & name
                switch (resultType)
                {
                case 1: // History          -> 0xe81c History
                    c->_iconPath = L"\ue81c";
                    break;
                case 2: // Command          -> 0xecaa AppIconDefault
                    c->_iconPath = L"\uecaa";
                    break;
                case 3: // ProviderItem     -> 0xe8e4 AlignLeft
                    c->_iconPath = L"\ue8e4";
                    break;
                case 4: // ProviderContainer  -> 0xe838 FolderOpen
                    c->_iconPath = L"\ue838";
                    break;
                case 5: // Property         -> 0xe7c1 Flag
                    c->_iconPath = L"\ue7c1";
                    break;
                case 6: // Method           -> 0xecaa AppIconDefault
                    c->_iconPath = L"\uecaa";
                    break;
                case 7: // ParameterName    -> 0xe7c1 Flag
                    c->_iconPath = L"\ue7c1";
                    break;
                case 8: // ParameterValue   -> 0xf000 KnowledgeArticle
                    c->_iconPath = L"\uf000";
                    break;
                case 10: // Namespace       -> 0xe943 Code
                    c->_iconPath = L"\ue943";
                    break;
                case 13: // DynamicKeyword  -> 0xe945 LightningBolt
                    c->_iconPath = L"\ue945";
                    break;
                }
            }

            result.push_back(*c);
        };

        if (root.isArray())
        {
            // If we got a whole array of suggestions, parse each one.
            for (const auto& element : root)
            {
                parseElement(element);
            }
        }
        else if (root.isObject())
        {
            // If we instead only got a single element back, just parse the root element.
            parseElement(root);
        }

        return winrt::single_threaded_vector<Model::Command>(std::move(result));
    }

    // Method description:
    // * Convert the list of recent commands into a list of sendInput actions to
    //   send those commands.
    // * We'll give each command a "history" icon.
    // * If directories is true, we'll prepend "cd " to each command, so that
    //   the command will be run as a directory change instead.
    IVector<Model::Command> Command::HistoryToCommands(IVector<winrt::hstring> history,
                                                       winrt::hstring currentCommandline,
                                                       bool directories,
                                                       winrt::hstring iconPath)
    {
        std::wstring cdText = directories ? L"cd " : L"";
        auto result = std::vector<Model::Command>();

        // Use this map to discard duplicates.
        std::unordered_map<std::wstring_view, bool> foundCommands{};

        auto backspaces = std::wstring(currentCommandline.size(), L'\x7f');

        // Iterate in reverse over the history, so that most recent commands are first
        for (auto i = history.Size(); i > 0; i--)
        {
            const auto& element{ history.GetAt(i - 1) };
            std::wstring_view line{ element };

            if (line.empty())
            {
                continue;
            }
            if (foundCommands.contains(line))
            {
                continue;
            }
            auto args = winrt::make_self<SendInputArgs>(
                winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}{}"), cdText, backspaces, line) });

            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, *args };

            auto command = winrt::make_self<Command>();
            command->_ActionAndArgs = actionAndArgs;
            command->_name = winrt::hstring{ line };
            command->_iconPath = iconPath;
            result.push_back(*command);
            foundCommands[line] = true;
        }

        return winrt::single_threaded_vector<Model::Command>(std::move(result));
    }

    void Command::LogSettingChanges(std::set<std::string>& changes)
    {
        if (_IterateOn != ExpandCommandType::None)
        {
            switch (_IterateOn)
            {
            case ExpandCommandType::Profiles:
                changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), IterateOnKey, "profiles"));
                break;
            case ExpandCommandType::ColorSchemes:
                changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), IterateOnKey, "schemes"));
                break;
            }
        }

        if (!_Description.empty())
        {
            changes.emplace(DescriptionKey);
        }

        if (IsNestedCommand())
        {
            changes.emplace(CommandsKey);
        }
        else
        {
            const auto json{ ActionAndArgs::ToJson(ActionAndArgs()) };
            if (json.isString())
            {
                // covers actions w/out args
                // - "command": "unbound" --> "unbound"
                // - "command": "copy"    --> "copy"
                changes.emplace(json.asString());
            }
            else
            {
                // covers actions w/ args
                // - "command": { "action": "copy", "singleLine": true }                           --> "copy.singleLine"
                // - "command": { "action": "copy", "singleLine": true, "dismissSelection": true } --> "copy.singleLine", "copy.dismissSelection"

                const std::string shortcutActionName{ json[JsonKey("action")].asString() };

                auto members = json.getMemberNames();
                members.erase(std::remove_if(members.begin(), members.end(), [](const auto& member) { return member == "action"; }), members.end());

                for (const auto& actionArg : members)
                {
                    changes.emplace(fmt::format(FMT_COMPILE("{}.{}"), shortcutActionName, actionArg));
                }
            }
        }
    }
}
