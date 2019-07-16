// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppUnitTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362. Until then, these
    // tests will be commented out. GH#1012 should move our CI to that version.
    // When that happens, these tests can be re-added.

    // class SettingsTests
    // {
    //     // Use a custom manifest to ensure that we can activate winrt types from
    //     // our test. This property will tell taef to manually use this as the
    //     // sxs manifest during this test class. It includes all the cppwinrt
    //     // types we've defined, so if your test is crashing for an unknown
    //     // reason, make sure it's included in that file.
    //     // If you want to do anything XAML-y, you'll need to run yor test in a
    //     // packaged context. See TabTests.cpp for more details on that.
    //     BEGIN_TEST_CLASS(SettingsTests)
    //         TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
    //     END_TEST_CLASS()

    //     TEST_METHOD(TryCreateWinRTType);
    // };

    // void SettingsTests::TryCreateWinRTType()
    // {
    //     winrt::Microsoft::Terminal::Settings::TerminalSettings settings{};
    //     VERIFY_IS_NOT_NULL(settings);
    //     auto oldFontSize = settings.FontSize();
    //     settings.FontSize(oldFontSize + 5);
    //     auto newFontSize = settings.FontSize();
    //     VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    // }

}
