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
            // TEST_CLASS_PROPERTY(L"ActivationContext", L"TerminalApp.Unit.Tests.manifest")
            // TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"PackagedCwaFullTrust")
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TerminalApp.Unit.Tests.AppxManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TryInitXamlIslands);
        TEST_METHOD(CreateLocalWinRTType);
        TEST_METHOD(CreateXamlObjects);
        TEST_METHOD(CreateDummyTab);

        TEST_CLASS_SETUP(ClassSetup)
        {
            reader = std::unique_ptr<Json::CharReader>(Json::CharReaderBuilder::CharReaderBuilder().newCharReader());

            winrt::init_apartment(winrt::apartment_type::single_threaded);
            // Initialize the Xaml Hosting Manager
            _manager = winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            _source = winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource{};

            return true;
        }

    private:
        std::unique_ptr<Json::CharReader> reader;

        winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager _manager{ nullptr };
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source{ nullptr };
    };

    void TabTests::TryInitXamlIslands()
    {
        // Ensures that XAML Islands was initialized correctly
        VERIFY_IS_NOT_NULL(_manager);
        VERIFY_IS_NOT_NULL(_source);
    }

    void TabTests::CreateLocalWinRTType()
    {
        // Verify we can create a WinRT type we authored
        winrt::Microsoft::Terminal::Settings::TerminalSettings settings{};
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    void TabTests::CreateXamlObjects()
    {
        // Verify we can create a some XAML objects
        winrt::Windows::UI::Xaml::Controls::UserControl controlRoot;
        VERIFY_IS_NOT_NULL(controlRoot);
        winrt::Windows::UI::Xaml::Controls::Grid root;
        VERIFY_IS_NOT_NULL(root);
        winrt::Windows::UI::Xaml::Controls::SwapChainPanel swapChainPanel;
        VERIFY_IS_NOT_NULL(swapChainPanel);
        winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar;
        VERIFY_IS_NOT_NULL(scrollBar);
    }

    void TabTests::CreateDummyTab()
    {
        const auto profileGuid{ Utils::CreateGuid() };
        winrt::Microsoft::Terminal::TerminalControl::TermControl term{};
        VERIFY_IS_NOT_NULL(term);

        auto newTab = std::make_shared<Tab>(profileGuid, term);

        VERIFY_IS_NOT_NULL(newTab);
    }



}
