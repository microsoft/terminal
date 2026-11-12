// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ResourceExecutor.h"
#include "JsonUtils.h"

namespace Microsoft::Terminal::Dsc::ResourceExecutor
{
    namespace
    {
        void writeJsonLine(std::ostream& output, const Json::Value& value)
        {
            output << SerializeJson(value) << '\n';
        }

        std::optional<Json::Value> parseOptionalInput(const std::optional<std::string>& input)
        {
            if (!input || input->empty())
            {
                return std::nullopt;
            }
            return ParseJson(*input);
        }

        Json::Value parseRequiredInput(const std::optional<std::string>& input, std::string_view operation)
        {
            if (!input || input->empty())
            {
                throw DscInputError{ fmt::format(FMT_COMPILE("'{}' requires a desired state; pass --input <json>"), operation) };
            }
            return ParseJson(*input);
        }
    }

    void ExecuteGet(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output)
    {
        writeJsonLine(output, registration.get->Get(parseOptionalInput(input)));
    }

    void ExecuteSet(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output)
    {
        const auto finalState{ registration.set->Set(parseRequiredInput(input, "set")) };
        if (registration.Metadata().setReturn != SetReturn::None)
        {
            writeJsonLine(output, finalState);
        }
    }

    void ExecuteTest(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output)
    {
        writeJsonLine(output, registration.test->Test(parseRequiredInput(input, "test")));
    }

    void ExecuteDelete(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& /*output*/)
    {
        registration.del->Delete(parseOptionalInput(input));
    }

    void ExecuteExport(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output)
    {
        for (const auto& instance : registration.exp->Export(parseOptionalInput(input)))
        {
            writeJsonLine(output, instance);
        }
    }

    void ExecuteSchema(const ResourceRegistration& registration, std::ostream& output)
    {
        writeJsonLine(output, registration.resource->Schema());
    }
}
