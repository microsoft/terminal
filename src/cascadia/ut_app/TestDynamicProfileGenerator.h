/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Profile.hpp

--*/

#include "../TerminalApp/IDynamicProfileGenerator.h"

namespace TerminalAppUnitTests
{
    class TestDynamicProfileGenerator;
};

class TerminalAppUnitTests::TestDynamicProfileGenerator final : public TerminalApp::IDynamicProfileGenerator
{
public:
    TestDynamicProfileGenerator(std::wstring_view ns) :
        _namespace{ ns } {};

    std::wstring_view GetNamespace() override { return _namespace; };

    std::vector<TerminalApp::Profile> GenerateProfiles() override
    {
        if (pfnGenerate)
        {
            return pfnGenerate();
        }
        return std::vector<TerminalApp::Profile>{};
    }

    std::wstring _namespace;

    std::function<std::vector<TerminalApp::Profile>()> pfnGenerate{ nullptr };
};
