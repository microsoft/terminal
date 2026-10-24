// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Logger.h"
#include "JsonUtils.h"

namespace Microsoft::Terminal::Dsc::Logger
{
    namespace
    {
        void writeLine(std::string_view level, std::string_view message)
        {
            Json::Value line{ Json::objectValue };
            line[std::string{ level }] = std::string{ message };
            std::cerr << SerializeJson(line) << '\n';
        }
    }

    void WriteInfo(std::string_view message)
    {
        writeLine("info", message);
    }

    void WriteWarning(std::string_view message)
    {
        writeLine("warn", message);
    }

    void WriteError(std::string_view message)
    {
        writeLine("error", message);
    }

    void WriteTrace(std::string_view message)
    {
        writeLine("trace", message);
    }
}
