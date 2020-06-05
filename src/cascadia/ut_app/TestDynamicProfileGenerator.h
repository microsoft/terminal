/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TestDynamicProfileGenerator.hpp

Abstract:
- This is a helper class for writing tests using dynamic profiles. Lets you
  easily set a arbitrary namespace and generation function for the profiles.

Author(s):
- Mike Griese - August 2019
--*/

#include "../TerminalApp/IDynamicProfileGenerator.h"

namespace TerminalAppUnitTests
{
    class TestDynamicProfileGenerator;
};

class TerminalAppUnitTests::TestDynamicProfileGenerator final :
    public TerminalApp::IDynamicProfileGenerator
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
