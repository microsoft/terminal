// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ResourceRegistry.h"

namespace Microsoft::Terminal::Dsc
{
    // Runs one operation against one registered resource and writes the
    // protocol output (compact JSON lines) to the stream. Capability presence
    // is validated by the caller; these throw DscInputError / DscResourceError
    // for input and state failures.
    namespace ResourceExecutor
    {
        void ExecuteGet(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output);
        void ExecuteSet(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output);
        void ExecuteTest(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output);
        void ExecuteDelete(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output);
        void ExecuteExport(const ResourceRegistration& registration, const std::optional<std::string>& input, std::ostream& output);
        void ExecuteSchema(const ResourceRegistration& registration, std::ostream& output);
    }
}
