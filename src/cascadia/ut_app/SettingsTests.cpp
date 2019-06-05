/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* Class Name: JsonTests
*/
#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppUnitTests
{
    class SettingsTests
    {
        BEGIN_TEST_CLASS(SettingsTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(CreateDummyWinRTType);

    };

    void SettingsTests::CreateDummyWinRTType()
    {
        winrt::Microsoft::Terminal::Settings::TerminalSettings settings{};
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

}
