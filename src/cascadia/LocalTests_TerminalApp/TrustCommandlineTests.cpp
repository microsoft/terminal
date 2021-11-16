// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// A series of tests for the TerminalPage::_isTrustedCommandline function.
// That's a heuristic function for deciding if we should automatically trust
// certain commandlines by default. The logic in there is weird and there are
// lots of edge cases, so it's easier to just write a unit test.

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
        TEST_METHOD(TestCommandlineWithEnvVars);
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

        Log::Comment(L"These are not fully qualified, and _shouldn't_ be trusted");
        VERIFY_IS_FALSE(trust(L"cmd.exe"));
        VERIFY_IS_FALSE(trust(L"powershell.exe"));
    }

    void TrustCommandlineTests::TestCommandlineWithArgs()
    {
        Log::Comment(L"These are sneaky things that _shouldn't_ be trusted");
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\cmd.exe /k echo Boo!"));
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\cmd.exe /k echo Boo! & cmd.exe"));
    }

    void TrustCommandlineTests::TestCommandlineWithSpaces()
    {
        Log::Comment(L"This is a valid place for powershell to live, and the space can be tricky");
        VERIFY_IS_TRUE(trust(L"C:\\Program Files\\PowerShell\\7\\pwsh.exe"));

        Log::Comment(L"These are sneaky things that _shouldn't_ be trusted");
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System 32\\cmd.exe"));
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\ cmd.exe"));
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\cmd.exe /c cmd.exe"));
    }

    void TrustCommandlineTests::TestCommandlineWithEnvVars()
    {
        Log::Comment(L"Make sure we auto-expand environment variables");

        VERIFY_IS_TRUE(trust(L"%WINDIR%\\system32\\cmd.exe"));
        VERIFY_IS_TRUE(trust(L"%WINDIR%\\system32\\WindowsPowerShell\\v1.0\\powershell.exe"));
        VERIFY_IS_TRUE(trust(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe"));
    }

    void TrustCommandlineTests::WslTests()
    {
        Log::Comment(L"We are explicitly deciding to not auto-approve "
                     L"`wsl.exe -d distro`-like commandlines. If we change this"
                     L" policy, remove this test.");

        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\wsl"));
        VERIFY_IS_TRUE(trust(L"C:\\Windows\\System32\\wsl.exe"), L"This we will trust though, since it's an exe in system32");
        VERIFY_IS_FALSE(trust(L"C:\\Windows\\System32\\wsl.exe -d Ubuntu"));
        VERIFY_IS_FALSE(trust(L"wsl.exe"));
    }

    void TrustCommandlineTests::TestPwshLocation()
    {
        Log::Comment(L"Test various locations that pwsh.exe can be in");

        VERIFY_IS_TRUE(trust(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe"));
        VERIFY_IS_TRUE(trust(L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\pwsh.exe"));
        VERIFY_IS_TRUE(trust(L"%ProgramFiles%\\PowerShell\\10\\pwsh.exe"));
        VERIFY_IS_TRUE(trust(L"%ProgramFiles%\\PowerShell\\7.1.5\\pwsh.exe"));

        Log::Comment(L"These are sneaky things that _shouldn't_ be trusted");
        VERIFY_IS_FALSE(trust(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe bad-stuff pwsh.exe"));
        VERIFY_IS_FALSE(trust(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe bad-stuff c:\\pwsh.exe"));
        VERIFY_IS_FALSE(trust(L"%ProgramFiles%\\PowerShell\\7\\pwsh.exe bad-stuff c:\\ %ProgramFiles%\\PowerShell\\7\\pwsh.exe"));
    }
}
