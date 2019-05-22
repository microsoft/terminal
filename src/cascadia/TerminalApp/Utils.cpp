// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"

std::wstring GetWstringFromJson(const Json::Value& json)
{
    return winrt::to_hstring(json.asString()).c_str();
}
