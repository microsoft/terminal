// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationStateSettings.h"
#include "Utils.h"
#include "JsonUtils.h"

using namespace TerminalApp;

Json::Value ApplicationStateSettings::ToJson() const
{
    return Json::Value::nullSingleton();
}

void ApplicationStateSettings::LayerJson(const Json::Value& value)
{
    JsonUtils::GetOptionalValue(value, "alwaysCloseAllTabs", _AlwaysCloseAllTabs, [](const Json::Value& val) {
        return val.asBool();
    });
}
