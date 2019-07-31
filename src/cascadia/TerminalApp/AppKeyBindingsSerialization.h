// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - AppKeyBindingsSerialization.h
//
// Abstract:
// - A couple helper functions for serializing/deserializing an AppKeyBindings
//   to/from json. We need this to exist as external helper functions, rather
//   than defining these as methods on the AppKeyBindings class, because
//   AppKeyBindings is a winrt type. When we're working with a AppKeyBindings
//   object, we only have access to methods defined on the winrt interface (in
//   the idl). We don't have access to methods we define on the
//   implementation. Since JsonValue is not a winrt type, we can't define any
//   methods that operate on it in the idl.
//
// Author(s):
// - Mike Griese - May 2019

#pragma once
#include "AppKeyBindings.h"

class AppKeyBindingsSerialization final
{
public:
    static winrt::TerminalApp::AppKeyBindings FromJson(const Json::Value& json);
    static Json::Value ToJson(const winrt::TerminalApp::AppKeyBindings& bindings);
};
