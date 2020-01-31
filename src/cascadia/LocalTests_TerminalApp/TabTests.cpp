// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/TerminalPage.h"
#include "../TerminalApp/MinMaxCloseControl.h"
#include "../TerminalApp/TabRowControl.h"
#include "../TerminalApp/ShortcutActionDispatch.h"
#include "../TerminalApp/Tab.h"
#include "../CppWinrtTailored.h"
#include "JsonTestClass.h"

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

    class TabTests : public JsonTestClass
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
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        // These four tests act as canary tests. If one of them fails, then they
        // can help you identify if something much lower in the stack has
        // failed.
        TEST_METHOD(EnsureTestsActivate);
        TEST_METHOD(TryCreateSettingsType);
        TEST_METHOD(TryCreateConnectionType);
        TEST_METHOD(TryCreateXamlObjects);
        TEST_METHOD(TryCreateTab);

        TEST_METHOD(CreateSimpleTerminalXamlType);
        TEST_METHOD(CreateTerminalMuxXamlType);

        TEST_METHOD(TryDuplicateBadTab);

        TEST_CLASS_SETUP(ClassSetup)
        {
            InitializeJsonReader();
            return true;
        }
    };

    void TabTests::EnsureTestsActivate()
    {
        // This test was originally used to ensure that XAML Islands was
        // initialized correctly. Now, it's used to ensure that the tests
        // actually deployed and activated. This test _should_ always pass.
        VERIFY_IS_TRUE(true);
    }

    void TabTests::TryCreateSettingsType()
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

    void TabTests::TryCreateConnectionType()
    {
        // Verify we can create a WinRT type we authored
        // Just creating it is enough to know that everything is working.
        winrt::Microsoft::Terminal::TerminalConnection::EchoConnection conn{};
        VERIFY_IS_NOT_NULL(conn);
        // We're doing this test seperately from the TryCreateSettingsType test,
        // to ensure both dependent binaries (TemrinalSettings and
        // TerminalConnection) both work individually.
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

    void TabTests::CreateSimpleTerminalXamlType()
    {
        winrt::com_ptr<winrt::TerminalApp::implementation::MinMaxCloseControl> mmcc{ nullptr };

        auto result = RunOnUIThread([&mmcc]() {
            mmcc = winrt::make_self<winrt::TerminalApp::implementation::MinMaxCloseControl>();
            VERIFY_IS_NOT_NULL(mmcc);
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::CreateTerminalMuxXamlType()
    {
        winrt::com_ptr<winrt::TerminalApp::implementation::TabRowControl> tabRowControl{ nullptr };

        auto result = RunOnUIThread([&tabRowControl]() {
            tabRowControl = winrt::make_self<winrt::TerminalApp::implementation::TabRowControl>();
            VERIFY_IS_NOT_NULL(tabRowControl);
        });
        VERIFY_SUCCEEDED(result);
    }

    void TabTests::TryDuplicateBadTab()
    {
        // Create a tab with a profile with GUID A
        // Reload the settings so that GUID A is no longer in the list of profiles
        // Try calling _DuplicateTabViewItem on tab A
        // No new tab should be created (and more importantly, the app should not crash)

        // This is a tests that was inspired by GH#2455, but at the time,
        // GH#2472 was still not solved, so this test was not possible to be
        // authored.

        const std::string settingsJson0{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        // const auto settings0JsonObj = VerifyParseSucceeded(settings0String);
        // auto settings0 = CascadiaSettings::FromJson(settings0JsonObj);

        const std::string settingsJson1{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        // const auto settings1JsonObj = VerifyParseSucceeded(settings1String);
        // auto settings1 = CascadiaSettings::FromJson(settings1JsonObj);

        VerifyParseSucceeded(settingsJson0);
        auto settings0 = std::make_shared<CascadiaSettings>(false);
        VERIFY_IS_NOT_NULL(settings0);
        settings0->_ParseJsonString(settingsJson0, false);
        settings0->LayerJson(settings0->_userSettings);
        settings0->_ValidateSettings();

        VerifyParseSucceeded(settingsJson1);
        auto settings1 = std::make_shared<CascadiaSettings>(false);
        VERIFY_IS_NOT_NULL(settings1);
        settings1->_ParseJsonString(settingsJson1, false);
        settings1->LayerJson(settings1->_userSettings);
        settings1->_ValidateSettings();

        const auto guid1 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        winrt::com_ptr<winrt::TerminalApp::implementation::TerminalPage> page{ nullptr };

        auto result = RunOnUIThread([&page, settings0]() {
            // auto foo{ winrt::make_self<winrt::TerminalApp::implementation::TerminalPage>() };
            winrt::TerminalApp::TerminalPage foo{};
            // VERIFY_IS_NOT_NULL(foo);
            page.copy_from(winrt::get_self<winrt::TerminalApp::implementation::TerminalPage>(foo));
            page->_settings = settings0;
        });
        VERIFY_SUCCEEDED(result);

        VERIFY_IS_NOT_NULL(page);
        VERIFY_IS_NOT_NULL(page->_settings);

        winrt::TerminalApp::TerminalPage projectedPage = *page;

        // page->Create();
        result = RunOnUIThread([&page]() {
            VERIFY_IS_NOT_NULL(page);
            VERIFY_IS_NOT_NULL(page->_settings);
            DebugBreak();
            page->Create();
        });
        VERIFY_SUCCEEDED(result);

        result = RunOnUIThread([&page]() {
            VERIFY_ARE_EQUAL(1u, page->_tabs.size());
        });
        VERIFY_SUCCEEDED(result);
    }

}
