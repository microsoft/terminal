// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Terminal::Dsc::Logger
{
    // Microsoft DSC reads diagnostics from stderr as JSON lines with a single
    // level key, e.g. {"error":"..."}.
    void WriteInfo(std::string_view message);
    void WriteWarning(std::string_view message);
    void WriteError(std::string_view message);
    void WriteTrace(std::string_view message);
}
