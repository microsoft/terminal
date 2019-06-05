/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* Class Name: JsonTests
*/
#include "precomp.h"

#include "../TerminalApp/ColorScheme.h"
#include "../TerminalApp/Tab.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalAppUnitTests
{
    class TabTests
    {
        BEGIN_TEST_CLASS(TabTests)
            TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
        END_TEST_CLASS()

        TEST_METHOD(CreateDummyTab);
        TEST_METHOD(TryInitXamlIslands);

        TEST_CLASS_SETUP(ClassSetup)
        {
            reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());
            // TODO: init xaml islands here, once we're sure that it works
            return true;
        }

        Json::Value VerifyParseSucceeded(std::string content);
        void VerifyParseFailed(std::string content);

    private:
        std::unique_ptr<Json::CharReader> reader;
        // TODO: Add this back as a member variable, once the test works
        // winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source{ nullptr };
    };

    void TabTests::TryInitXamlIslands()
    {
        // DebugBreak();
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source{ nullptr };
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        // Initialize the Xaml Hosting Manager
        auto manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        _source = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource{};
    }

    void TabTests::CreateDummyTab()
    {
        // This test won't work if the TryInitXamlIslands test fails. We'll
        // remove that test once we have xaml islands working.
        const auto profileGuid{ Utils::CreateGuid() };
        winrt::Microsoft::Terminal::TerminalControl::TermControl term{ nullptr };

        auto newTab = std::make_shared<Tab>(profileGuid, term);

        VERIFY_IS_NOT_NULL(newTab);
    }



}
