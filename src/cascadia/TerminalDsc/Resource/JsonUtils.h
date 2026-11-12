// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Terminal::Dsc
{
    // Serializes a JSON value into a compact single line, the shape Microsoft
    // DSC expects on stdout.
    std::string SerializeJson(const Json::Value& value);

    // Parses strict JSON; throws DscInputError on malformed input.
    Json::Value ParseJson(std::string_view text);
}
