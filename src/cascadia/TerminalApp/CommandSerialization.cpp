// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandSerialization.h"
#include "Utils.h"
#include "ActionAndArgs.h"
#include <LibraryResources.h>

using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconPathKey{ "iconPath" };
static constexpr std::string_view ActionKey{ "action" };
static constexpr std::string_view ArgsKey{ "args" };

// Method Description:
// - Deserialize an AppKeyBindings from the key mappings that are in the array
//   `json`. The json array should contain an array of objects with both a
//   `command` string and a `keys` array, where `command` is one of the names
//   listed in `commandNames`, and `keys` is an array of keypresses. Currently,
//   the array should contain a single string, which can be deserialized into a
//   KeyChord.
// Arguments:
// - json: and array of JsonObject's to deserialize into our _keyShortcuts mapping.
// Return Value:
// - the newly constructed AppKeyBindings object.
winrt::TerminalApp::Command CommandSerialization::FromJson(const Json::Value& json)
{
    winrt::TerminalApp::Command result{};

    if (auto name{ json[JsonKey(NameKey)] })
    {
        if (name.isObject())
        {
            try
            {
                if (auto keyJson{ name[JsonKey("key")] })
                {
                    auto resourceKey = GetWstringFromJson(keyJson);
                    result.Name(GetLibraryResourceString(resourceKey));
                }
            }
            CATCH_LOG();
        }
        else if (name.isString())
        {
            result.Name(winrt::to_hstring(name.asString()));
        }
    }
    if (auto iconPath{ json[JsonKey(IconPathKey)] })
    {
        result.IconPath(winrt::to_hstring(iconPath.asString()));
    }

    // Ask the keybinding serializer to turn us into a ActionAndArgs
    std::vector<::TerminalApp::SettingsLoadWarnings> warnings;
    auto actionAndArgs = winrt::TerminalApp::implementation::ActionAndArgs::FromJson(json, warnings);

    if (actionAndArgs)
    {
        result.Action(*actionAndArgs);
    }
    // else
    // {
    //     ClearKeyBinding(chord);
    // }

    return result;
}

void CommandSerialization::LayerJson(std::vector<winrt::TerminalApp::Command>& commands,
                                     const Json::Value& json)
{
    for (const auto& value : json)
    {
        if (value.isObject())
        {
            try
            {
                commands.push_back(CommandSerialization::FromJson(value));
            }
            CATCH_LOG();
        }
    }
}
