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
        // For this set of tests, we need to activate some XAML content. To do
        // that, we need to be able to activate Xaml Islands(XI), using the Xaml
        // Hosting APIs. Because XI looks at the manifest of the exe running, we
        // can't just use the TerminalApp.Unit.Tests.manifest as our
        // ActivationContext. XI is going to inspect `te.exe`s manifest to try
        // and find the maxversiontested property, but te.exe hasn't set that.
        // Instead, this test will run as a UAP application, as a packaged
        // centenial (win32) app. We'll specify our own AppxManifest, so that
        // we'll be able to also load all the dll's for the types we've defined
        // (and want to use here). This does come with a minor caveat, as
        // deploying the appx takes a bit, so use sparingly (though it will
        // deploy once per class when used like this.)
        BEGIN_TEST_CLASS(TabTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.LocalTests.AppxManifest.xml")
        END_TEST_CLASS()

        // These four tests act as canary tests. If one of them fails, then they
        // can help you identify if something much lower in the stack has
        // failed.
        TEST_METHOD(TryInitXamlIslands);
        TEST_METHOD(TryCreateLocalWinRTType);
        TEST_METHOD(TryCreateXamlObjects);
        TEST_METHOD(TryCreateTab);

        TEST_CLASS_SETUP(ClassSetup)
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            // Initialize the Xaml Hosting Manager
            _manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            _source = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource{};

            return true;
        }

    private:
        winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager _manager{ nullptr };
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source{ nullptr };
    };

    void TabTests::TryInitXamlIslands()
    {
        // Ensures that XAML Islands was initialized correctly
        VERIFY_IS_NOT_NULL(_manager);
        VERIFY_IS_NOT_NULL(_source);
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
