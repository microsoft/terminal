# -*- coding: utf-8 -*-
import os, sys
from os import path
from datetime import datetime
import json
# from json import JSONEncoder
from collections import namedtuple

# TODO:
# * [x] ActionAndArg.cpp
# * [ ] ActionArgs.h
# * [ ] includes in ActionArgs.cpp
# * [ ] ShortcutActionDispatch.idl
# * [ ] ShortcutActionDispatch.h
# * [ ] ShortcutActionDispatch.cpp
# * [ ] TerminalPage.h (action handlers)
# * [ ] TerminalPage.cpp (hookup of action handlers)
# * [ ] Save each of these to a reasonable place
# DON'T GENERATE:
# * resw
# * GenerateName (in ActionArgs.cpp)



__author__ = 'zadjii'

ResultAndData = namedtuple('ResultAndData', 'success, data')

def Error(data=None):
    return ResultAndData(False, data)
def Success(data=None):
    return ResultAndData(True, data)

TERMINAL_ROOT = path.abspath(path.dirname(path.dirname(__file__)))
SCHEMA_ROOT = os.path.join(TERMINAL_ROOT, 'doc/cascadia/profiles.schema.json')


def do_load_backend():
    #type () -> ResultAndData
    #type () -> Error(str)
    #type () -> Success(json)
    backend_path = SCHEMA_ROOT # os.path.join(SCHEMA_ROOT, 'backend')
    if not os.path.exists(backend_path):
        return Error('File didn\'t exist')

    with open(backend_path, mode='r', encoding='utf-8') as f:
        file_contents = f.read()
        backend_dict = {}
        if len(file_contents) > 0:
            backend_dict = json.loads(file_contents)
        else:
            return Error('File was empty')

        # serialized_obj = RootModel.deserialize(backend_dict)
        # return Success(serialized_obj)
        return Success(backend_dict)

actions_and_properties = {}

################################################################################
# 4 things to fill in
KEY_STRING_TMPL = 'static constexpr std::string_view {}Key{{ "{}" }};\n'
KEY_NAMES_MAP_TMPL = '        {{ {}Key, ShortcutAction::{} }},\n'
FROM_JSON_TMPL = '        {{ ShortcutAction::{}, winrt::TerminalApp::implementation::{}Args::FromJson }},\n'
GENERATE_NAME_TMPL = '                {{ ShortcutAction::{}, RS_(L"{}CommandKey") }},\n'

ActionAndArgs_cpp = """
#include "pch.h"
#include "ActionArgs.h"
#include "ActionAndArgs.h"
#include "ActionAndArgs.g.cpp"

#include "JsonUtils.h"

#include <LibraryResources.h>

%s

static constexpr std::string_view ActionKey{ "action" };

// This key is reserved to remove a keybinding, instead of mapping it to an action.
static constexpr std::string_view UnboundKey{ "unbound" };

namespace winrt::TerminalApp::implementation
{
    using namespace ::TerminalApp;

    // Specifically use a map here over an unordered_map. We want to be able to
    // iterate over these entries in-order when we're serializing the keybindings.
    // HERE BE DRAGONS:
    // These are string_views that are being used as keys. These string_views are
    // just pointers to other strings. This could be dangerous, if the map outlived
    // the actual strings being pointed to. However, since both these strings and
    // the map are all const for the lifetime of the app, we have nothing to worry
    // about here.
    const std::map<std::string_view, ShortcutAction, std::less<>> ActionAndArgs::ActionKeyNamesMap{
%s
        { UnboundKey, ShortcutAction::Invalid }
    };

    using ParseResult = std::tuple<IActionArgs, std::vector<::TerminalApp::SettingsLoadWarnings>>;
    using ParseActionFunction = std::function<ParseResult(const Json::Value&)>;

    // This is a map of ShortcutAction->function<IActionArgs(Json::Value)>. It holds
    // a set of deserializer functions that can be used to deserialize a IActionArgs
    // from json. Each type of IActionArgs that can accept arbitrary args should be
    // placed into this map, with the corresponding deserializer function as the
    // value.
    static const std::map<ShortcutAction, ParseActionFunction, std::less<>> argParsers{
%s
        { ShortcutAction::Invalid, nullptr },
    };

    // Function Description:
    // - Attempts to match a string to a ShortcutAction. If there's no match, then
    //   returns ShortcutAction::Invalid
    // Arguments:
    // - actionString: the string to match to a ShortcutAction
    // Return Value:
    // - The ShortcutAction corresponding to the given string, if a match exists.
    static ShortcutAction GetActionFromString(const std::string_view actionString)
    {
        // Try matching the command to one we have. If we can't find the
        // action name in our list of names, let's just unbind that key.
        const auto found = ActionAndArgs::ActionKeyNamesMap.find(actionString);
        return found != ActionAndArgs::ActionKeyNamesMap.end() ? found->second : ShortcutAction::Invalid;
    }

    // Method Description:
    // - Deserialize an ActionAndArgs from the provided json object or string `json`.
    //   * If json is a string, we'll attempt to treat it as an action name,
    //     without arguments.
    //   * If json is an object, we'll attempt to retrieve the action name from
    //     its "action" property, and we'll use that name to fine a deserializer
    //     to precess the rest of the arguments in the json object.
    // - If the action name is null or "unbound", or we don't understand the
    //   action name, or we failed to parse the arguments to this action, we'll
    //   return null. This should indicate to the caller that the action should
    //   be unbound.
    // - If there were any warnings while parsing arguments for the action,
    //   they'll be appended to the warnings parameter.
    // Arguments:
    // - json: The Json::Value to attempt to parse as an ActionAndArgs
    // - warnings: If there were any warnings during parsing, they'll be
    //   appended to this vector.
    // Return Value:
    // - a deserialized ActionAndArgs corresponding to the values in json, or
    //   null if we failed to deserialize an action.
    winrt::com_ptr<ActionAndArgs> ActionAndArgs::FromJson(const Json::Value& json,
                                                          std::vector<::TerminalApp::SettingsLoadWarnings>& warnings)
    {
        // Invalid is our placeholder that the action was not parsed.
        ShortcutAction action = ShortcutAction::Invalid;

        // Actions can be serialized in two styles:
        //   "action": "switchToTab0",
        //   "action": { "action": "switchToTab", "index": 0 },
        // NOTE: For keybindings, the "action" param is actually "command"

        // 1. In the first case, the json is a string, that's the
        //    action name. There are no provided args, so we'll pass
        //    Json::Value::null to the parse function.
        // 2. In the second case, the json is an object. We'll use the
        //    "action" in that object as the action name. We'll then pass
        //    the json object to the arg parser, for further parsing.

        auto argsVal = Json::Value::null;

        // Only try to parse the action if it's actually a string value.
        // `null` will not pass this check.
        if (json.isString())
        {
            auto commandString = json.asString();
            action = GetActionFromString(commandString);
        }
        else if (json.isObject())
        {
            if (const auto actionString{ JsonUtils::GetValueForKey<std::optional<std::string>>(json, ActionKey) })
            {
                action = GetActionFromString(*actionString);
                argsVal = json;
            }
        }

        // Some keybindings can accept other arbitrary arguments. If it
        // does, we'll try to deserialize any "args" that were provided with
        // the binding.
        IActionArgs args{ nullptr };
        std::vector<::TerminalApp::SettingsLoadWarnings> parseWarnings;
        const auto deserializersIter = argParsers.find(action);
        if (deserializersIter != argParsers.end())
        {
            auto pfn = deserializersIter->second;
            if (pfn)
            {
                std::tie(args, parseWarnings) = pfn(argsVal);
            }
            warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());

            // if an arg parser was registered, but failed, bail
            if (pfn && args == nullptr)
            {
                return nullptr;
            }
        }

        if (action != ShortcutAction::Invalid)
        {
            auto actionAndArgs = winrt::make_self<ActionAndArgs>();
            actionAndArgs->Action(action);
            actionAndArgs->Args(args);

            return actionAndArgs;
        }
        else
        {
            return nullptr;
        }
    }

    winrt::hstring ActionAndArgs::GenerateName() const
    {
        // Use a magic static to initialize this map, because we won't be able
        // to load the resources at _init_, only at runtime.
        static const auto GeneratedActionNames = []() {
            return std::unordered_map<ShortcutAction, winrt::hstring>{
%s
                { ShortcutAction::Invalid, L"" },
            };
        }();

        if (_Args)
        {
            auto nameFromArgs = _Args.GenerateName();
            if (!nameFromArgs.empty())
            {
                return nameFromArgs;
            }
        }

        const auto found = GeneratedActionNames.find(_Action);
        return found != GeneratedActionNames.end() ? found->second : L"";
    }
}
"""

################################################################################

KEY_STRING_TMPL = 'static constexpr std::string_view {}Key{{ "{}" }};\n'
KEY_NAMES_MAP_TMPL = '        {{ {}Key, ShortcutAction::{} }},\n'
FROM_JSON_TMPL = '        {{ ShortcutAction::{}, winrt::TerminalApp::implementation::{}Args::FromJson }},\n'
GENERATE_NAME_TMPL = '                {{ ShortcutAction::{}, RS_(L"{}CommandKey") }},\n'

ActionArgs_h = """
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
%s

#include "../../cascadia/inc/cppwinrt_utils.h"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalWarnings.h"

#include "TerminalSettingsSerializationHelpers.h"

namespace winrt::TerminalApp::implementation
{
%s
}
"""

ActionArgs_tmpl_1 = '        static constexpr std::string_view {0}Key{{ "{1}" }};'
ActionArgs_tmpl_2 = 'otherAsUs->_{0} == _{0}'
ActionArgs_tmpl_3 = '            JsonUtils::GetValueForKey(json, {0}Key, args->_{0});'
ActionArgs_tmpl = """
    struct {0}Args : public {0}ArgsT<{0}Args>
    {{
        {0}Args() = default;
        GETSET_PROPERTY(bool, SingleLine, false);

{1}
    public:
        hstring GenerateName() const;

        bool Equals(const IActionArgs& other)
        {{
            auto otherAsUs = other.try_as<{0}Args>();
            if (otherAsUs)
            {{
                return {2};
            }}
            return false;
        }};
        static FromJsonResult FromJson(const Json::Value& json)
        {{
            // LOAD BEARING: Not using make_self here _will_ break you in the future!
            auto args = winrt::make_self<{0}Args>();
{3}
            return {{ *args, {{}} }};
        }}
    }};

"""

def get_name_string(action_string):
    props = actions_and_properties[action_string]
    name = None
    if props and 'name' in props:
        name = props['name']
    else:
        action_string[0].capitalize()
        name = action_string[0].capitalize() + action_string[1:]
        return name

def do_ActionArgs_h():
    args_headers = ''
    all_args = ''

    for action_string in actions_and_properties.keys():
        props = actions_and_properties[action_string]
        if props is None:
            continue

        name = get_name_string(action_string)

        getset_props = ''
        equality = ''
        deserializer = ''

        for prop_name in props.keys():
            if prop_name != 'action' and prop_name != 'name':
                prop = props[prop_name]
                prop_type = None
                if 'type' in prop:
                    prop_type = prop['type']
                elif '$ref' in prop:
                    prop_type = prop['$ref']
                else:
                    prop_type = 'FILL_THIS_IN_MANUALLY'
                print(f'{prop_name}:{prop_type}')


        complete_arg = ActionArgs_tmpl.format(name, getset_props, equality, deserializer)
        all_args += complete_arg
    return ActionArgs_h % (args_headers, all_args)

def do_ActionAndArgs_cpp():
    key_strings = ''
    ActionKeyNamesMap = ''
    argParsers = ''
    GeneratedActionNames = ''

    for action_string in actions_and_properties.keys():
        props = actions_and_properties[action_string]
        name = get_name_string(action_string)

        # print(f'{action_string}->{props}')
        this_key_string = KEY_STRING_TMPL.format(name, action_string)
        key_strings += this_key_string

        ActionKeyNamesMap += KEY_NAMES_MAP_TMPL.format(name, name)

        if props is not None:
            argParsers += FROM_JSON_TMPL.format(name, name)

        GeneratedActionNames += GENERATE_NAME_TMPL.format(name, name)
    # print(key_strings)
    # print(ActionKeyNamesMap)
    # print(argParsers)
    # print(GeneratedActionNames)
    return ActionAndArgs_cpp % (key_strings, ActionKeyNamesMap, argParsers, GeneratedActionNames)


def main():
    # print(TERMINAL_ROOT)
    # print(SCHEMA_ROOT)

    rd = do_load_backend()
    if not rd.success:
        print('Failed to load schema file')
        print(rd.data)
        exit(-1)

    json_dict = rd.data
    definitions = json_dict["definitions"]
    # for d in definitions.keys():
    #     print(f'\t{d}')

    all_actions = definitions["ShortcutActionName"]["enum"]
    all_actions = [action for action in all_actions if action != 'unbound']
    for action in all_actions:
        actions_and_properties[action] = None

    # get all the ActionArgs
    actions_args = definitions['Keybinding']['properties']['command']['oneOf']

    # get only the ones that have definitions - `null` won't pass this
    actions_args = [aa['$ref'] for aa in actions_args if '$ref' in aa]

    # get only the action name
    actions_args = [aa.split('/')[-1] for aa in actions_args]

    # remove ShortcutActionName, since we don't need that one
    actions_args = [aa for aa in actions_args if aa != 'ShortcutActionName']

    actions_arg_defs = [k for k in definitions.keys() if k in actions_args]
    for action_name in actions_arg_defs:
        action_definition = definitions[action_name]
        action_props = action_definition['allOf'][-1]['properties']
        action_string = action_props['action']['pattern']
        action_props['name'] = action_name
        actions_and_properties[action_string] = action_props


    # for action_string in actions_and_properties.keys():
    #     props = actions_and_properties[action_string]
    #     print(f'{action_string}->{props}')
    # do_ActionAndArgs_cpp()
    do_ActionArgs_h()

if __name__ == '__main__':
    main()
