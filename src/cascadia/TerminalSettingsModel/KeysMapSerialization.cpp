// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// - A couple helper functions for serializing/deserializing a KeyMapping
//   to/from json.
//
// Author(s):
// - Mike Griese - May 2019

#include "pch.h"
#include "KeysMap.h"
#include "ActionAndArgs.h"
#include "KeyChordSerialization.h"
#include "JsonUtils.h"

#include "Command.h"

using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    void KeysMap::Ayy()
    {
        return;
    }

    // Method Description:
    // - Deserialize an ActionMap from the array `json`. The json array should contain
    //   an array of serialized `Command` objects.
    // - These actions are added to the `ActionMap`, where we automatically handle
    //   overwriting and unbinding actions.
    // Arguments:
    // - json: an array of Json::Value's to deserialize into our ActionMap.
    // Return value:
    // - a list of warnings encountered while deserializing the json
    std::vector<SettingsLoadWarnings> KeysMap::LayerJson(const Json::Value& /*json*/, const OriginTag /*origin*/, const bool /*withKeybindings*/)
    {
        // It's possible that the user provided keybindings have some warnings in
        // them - problems that we should alert the user to, but we can recover
        // from. Most of these warnings cannot be detected later in the Validate
        // settings phase, so we'll collect them now.
        std::vector<SettingsLoadWarnings> warnings;

        // todo: complete this

        return warnings;
    }
}
