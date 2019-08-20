// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

class JsonTestClass
{
public:
    void InitializeJsonReader()
    {
        reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());
    };
    Json::Value VerifyParseSucceeded(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    };

protected:
    std::unique_ptr<Json::CharReader> reader;
};
