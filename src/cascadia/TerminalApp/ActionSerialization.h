// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ActionSerialization.h
//
// Abstract:
// - A couple helper functions for serializing/deserializing an Actions
//   to/from json. We need this to exist as external helper functions, rather
//   than defining these as methods on the Action class, because
//   Action is a winrt type. When we're working with an Action
//   object, we only have access to methods defined on the winrt interface (in
//   the idl). We don't have access to methods we define on the
//   implementation. Since JsonValue is not a winrt type, we can't define any
//   methods that operate on it in the idl.
//
// Author(s):
// - Mike Griese - July 2019

#pragma once
#include "Action.h"

class ActionSerialization final
{
public:
    static winrt::TerminalApp::Action FromJson(const Json::Value& json);
    static Json::Value ToJson(const winrt::TerminalApp::Action& action);
};
