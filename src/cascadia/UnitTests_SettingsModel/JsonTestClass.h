/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonTestClass.h

Abstract:
- This class is a helper that can be used to quickly create tests that need to
  read & parse json data.

Author(s):
    Mike Griese (migrie) August-2019
--*/

#pragma once

class JsonTestClass
{
public:
    static Json::Value VerifyParseSucceeded(const std::string_view& content)
    {
        static const std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };

        Json::Value root;
        std::string errs;
        const auto parseResult = reader->parse(content.data(), content.data() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    };

    static std::string toString(const Json::Value& json)
    {
        static const std::unique_ptr<Json::StreamWriter> writer{ Json::StreamWriterBuilder{}.newStreamWriter() };

        std::stringstream s;
        writer->write(json, &s);
        return s.str();
    }
};
