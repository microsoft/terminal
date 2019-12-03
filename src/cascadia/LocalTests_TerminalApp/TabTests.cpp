// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Tab.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppLocalTests
{
    // Unfortunately, these tests _WILL NOT_ work in our CI, until we have a lab
    // machine available that can run Windows version 18362.

    class TabTests
    {
        // For this set of tests, we need to activate some XAML content. For
        // release builds, the application runs as a centennial application,
        // which lets us run full trust, and means that we need to use XAML
        // Islands to host our UI. However, in these tests, we don't really need
        // to run full trust - we just need to get some UI elements created. So
        // we can just rely on the normal UWP activation to create us.
        // UNFORTUNATELY, this doesn't seem to work yet, and the tests fail when
        // instantiating XAML elements

        BEGIN_TEST_CLASS(TabTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:WaitForXamlWindowActivation", L"true")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.LocalTests.AppxManifest.xml")
        END_TEST_CLASS()

        // These four tests act as canary tests. If one of them fails, then they
        // can help you identify if something much lower in the stack has
        // failed.
        TEST_METHOD(EnsureTestsActivate);
        TEST_METHOD(TryCreateLocalWinRTType);
        TEST_METHOD(TryCreateXamlObjects);
        TEST_METHOD(TryCreateTab);
    };

    void TabTests::EnsureTestsActivate()
    {
        // This test was originally used to ensure that XAML Islands was
        // initialized correctly. Now, it's used to ensure that the tests
        // actually deployed and activated. This test _should_ always pass.
        VERIFY_IS_TRUE(true);
    }

    void TabTests::TryCreateLocalWinRTType()
    {
        // Verify we can create a WinRT type we authored
        // Just creating it is enough to know that everything is working.
        winrt::Microsoft::Terminal::Settings::TerminalSettings settings;
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    void TabTests::TryCreateXamlObjects()
    {
        // Verify we can create a some XAML objects
        // Just creating all of them is enough to know that everything is working.
        winrt::Windows::UI::Xaml::Controls::UserControl controlRoot;
        VERIFY_IS_NOT_NULL(controlRoot);
        winrt::Windows::UI::Xaml::Controls::Grid root;
        VERIFY_IS_NOT_NULL(root);
        winrt::Windows::UI::Xaml::Controls::SwapChainPanel swapChainPanel;
        VERIFY_IS_NOT_NULL(swapChainPanel);
        winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar;
        VERIFY_IS_NOT_NULL(scrollBar);
    }

    void TabTests::TryCreateTab()
    {
        // Just try creating all of:
        // 1. one of our pure c++ types (Profile)
        // 2. one of our c++winrt types (TermControl)
        // 3. one of our types that uses MUX/Xaml (Tab).
        // Just creating all of them is enough to know that everything is working.
        const auto profileGuid{ Utils::CreateGuid() };
        winrt::Microsoft::Terminal::TerminalControl::TermControl term{};
        VERIFY_IS_NOT_NULL(term);

        auto newTab = std::make_shared<Tab>(profileGuid, term);

        VERIFY_IS_NOT_NULL(newTab);
    }

}
