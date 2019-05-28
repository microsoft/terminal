// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"

// Method Description:
// - Contstructs a wstring from a given Json::Value object. Reads the object as
//   a std::string using asString, then builds an hstring from that std::string,
//   then converts that hstring into a std::wstring.
// Arguments:
// - json: the Json::Value to parse as a string
// Return Value:
// - the wstring equivalent of the value in json
std::wstring GetWstringFromJson(const Json::Value& json)
{
    return winrt::to_hstring(json.asString()).c_str();
}
