// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/TerminalPage.h"

using namespace Microsoft::Console;
using namespace TerminalApp;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings::Model;

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace TerminalAppLocalTests
{
    class TrustCommandlineTests
    {
        BEGIN_TEST_CLASS(TrustCommandlineTests)
        END_TEST_CLASS()

        TEST_METHOD(SimpleTests);
        TEST_METHOD(TestCommandlineWithArgs);
        TEST_METHOD(TestCommandlineWithSpaces);
        TEST_METHOD(WslTests);
        TEST_METHOD(TestPwshLocation);

        bool trust(std::wstring_view cmdline);
    };

    bool TrustCommandlineTests::trust(std::wstring_view cmdline)
    {
        return implementation::TerminalPage::_isTrustedCommandline(cmdline);
    }

    void TrustCommandlineTests::SimpleTests()
    {
        VERIFY_IS_TRUE(trust(L"C:\\Windows\\System32\\cmd.exe"));
        VERIFY_IS_TRUE(trust(L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"));
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\i-definitely-don't-exist.exe"));

        VERIFY_IS_FALSE(trust(L"cmd.exe"));
        VERIFY_IS_FALSE(trust(L"powershell.exe"));
    }

    void TrustCommandlineTests::TestCommandlineWithArgs()
    {
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\cmd.exe /k echo Boo!"));
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\cmd.exe /k echo Boo! & cmd.exe"));
    }

    void TrustCommandlineTests::TestCommandlineWithSpaces()
    {
        VERIFY_IS_TRUE(false, L"TODO! implement me.");
    }
    void TrustCommandlineTests::WslTests()
    {
        VERIFY_IS_TRUE(false, L"TODO! implement me.");
    }
    void TrustCommandlineTests::TestPwshLocation()
    {
        VERIFY_IS_TRUE(false, L"TODO! implement me.");
    }
}
