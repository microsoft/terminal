// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "AppKeyBindings.h"

class AppKeyBindingsSerialization final
{
public:
    static winrt::TerminalApp::AppKeyBindings FromJson(const Json::Value& json);
    static Json::Value ToJson(const winrt::TerminalApp::AppKeyBindings& bindings);
};
