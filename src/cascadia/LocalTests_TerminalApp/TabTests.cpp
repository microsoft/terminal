// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Tab.h"
#include "../CppWinrtTailored.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class TabTests
    {
        // For this set of tests, we need to activate some XAML content. For
        // release builds, the application runs as a centennial application,
        // which lets us run full trust, and means that we need to use XAML
        // Islands to host our UI. However, in these tests, we don't really need
        // to run full trust - we just need to get some UI elements created. So
        // we can just rely on the normal UWP activation to create us.
        //
        // IMPORTANTLY! When tests need to make XAML objects, or do XAML things,
        // make sure to use RunOnUIThread. This helper will dispatch a lambda to
        // be run on the UI thread.

        BEGIN_TEST_CLASS(TabTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.LocalTests.AppxManifest.xml")
            TEST_CLASS_PROPERTY(L"UAP:Host", L"Xaml")
            TEST_CLASS_PROPERTY(L"UAP:WaitForXamlWindowActivation", L"true")
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
        auto result = RunOnUIThread([]() {
            VERIFY_IS_TRUE(true, L"Congrats! We're running on the UI thread!");

            auto v = winrt::Windows::ApplicationModel::Core::CoreApplication::GetCurrentView();
            VERIFY_IS_NOT_NULL(v, L"Ensure we have a current view");
            // Verify we can create a some XAML objects
            // Just creating all of them is enough to know that everything is working.
            winrt::Windows::UI::Xaml::Controls::UserControl controlRoot;
            VERIFY_IS_NOT_NULL(controlRoot, L"Try making a UserControl");
            winrt::Windows::UI::Xaml::Controls::Grid root;
            VERIFY_IS_NOT_NULL(root, L"Try making a Grid");
            winrt::Windows::UI::Xaml::Controls::SwapChainPanel swapChainPanel;
            VERIFY_IS_NOT_NULL(swapChainPanel, L"Try making a SwapChainPanel");
            winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar;
            VERIFY_IS_NOT_NULL(scrollBar, L"Try making a ScrollBar");
        });

        VERIFY_SUCCEEDED(result);
    }

    void TabTests::TryCreateTab()
    {
        // If you leave the Tab shared_ptr owned by the RunOnUIThread lambda, it
        // will crash when the test tears down. Not totally clear why, but make
        // sure it's owned outside the lambda
        std::shared_ptr<Tab> newTab{ nullptr };

        auto result = RunOnUIThread([&newTab]() {
            // Try creating all of:
            // 1. one of our pure c++ types (Profile)
            // 2. one of our c++winrt types (TerminalSettings, EchoConnection)
            // 3. one of our types that uses MUX/Xaml (TermControl).
            // 4. one of our types that uses MUX/Xaml in this dll (Tab).
            // Just creating all of them is enough to know that everything is working.
            const auto profileGuid{ Utils::CreateGuid() };
            winrt::Microsoft::Terminal::Settings::TerminalSettings settings{};
            VERIFY_IS_NOT_NULL(settings);
            winrt::Microsoft::Terminal::TerminalConnection::EchoConnection conn{};
            VERIFY_IS_NOT_NULL(conn);
            winrt::Microsoft::Terminal::TerminalControl::TermControl term{ settings, conn };
            VERIFY_IS_NOT_NULL(term);

            newTab = std::make_shared<Tab>(profileGuid, term);
            VERIFY_IS_NOT_NULL(newTab);
        });

        VERIFY_SUCCEEDED(result);
    }

}
