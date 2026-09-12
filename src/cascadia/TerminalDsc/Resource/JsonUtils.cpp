// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "JsonUtils.h"
#include "DscResource.h"

namespace Microsoft::Terminal::Dsc
{
    std::string SerializeJson(const Json::Value& value)
    {
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        builder.settings_["commentStyle"] = "None";
        return Json::writeString(builder, value);
    }

    Json::Value ParseJson(std::string_view text)
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        const std::unique_ptr<Json::CharReader> reader{ builder.newCharReader() };
        Json::Value root;
        std::string errors;
        if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors))
        {
            throw DscInputError{ fmt::format(FMT_COMPILE("invalid JSON input: {}"), errors) };
        }
        return root;
    }
}
