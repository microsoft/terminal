/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonTestClass.h

Abstract:
- This class is a helper that can be used to quickly create tests that need to
  read & parse json data. Test classes that need to read JSON should make sure
  to derive from this class, and also make sure to call InitializeJsonReader()
  in the TEST_CLASS_SETUP().

Author(s):
    Mike Griese (migrie) August-2019
--*/

class JsonTestClass
{
public:
    void InitializeJsonReader()
    {
        _reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());
    };

    void InitializeJsonWriter()
    {
        _writer = std::unique_ptr<Json::StreamWriter>(Json::StreamWriterBuilder::StreamWriterBuilder().newStreamWriter());
    }

    Json::Value VerifyParseSucceeded(std::string content)
    {
        Json::Value root;
        std::string errs;
        const bool parseResult = _reader->parse(content.c_str(), content.c_str() + content.size(), &root, &errs);
        VERIFY_IS_TRUE(parseResult, winrt::to_hstring(errs).c_str());
        return root;
    };

    std::string toString(const Json::Value& json)
    {
        std::stringstream s;
        _writer->write(json, &s);
        return s.str();
    }

protected:
    std::unique_ptr<Json::CharReader> _reader;
    std::unique_ptr<Json::StreamWriter> _writer;
};
