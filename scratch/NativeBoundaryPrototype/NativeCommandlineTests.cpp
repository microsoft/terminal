// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "CommandlineArgs.h"

#include <cassert>
#include <iterator>
#include <windows.h>

using TerminalApp::Native::CommandlineArgs;
using TerminalApp::Native::CommandlineArgsRef;

void TestProjectedCommandlineBridge(const CommandlineArgsRef& value);

int main()
{
    winrt::init_apartment();
    const auto module = GetModuleHandleW(L"TerminalApp.dll");
    assert(module);
    const auto canUnload = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(module, "DllCanUnloadNow"));
    assert(canUnload);
    assert(canUnload() == S_OK);
    {
        auto args = CommandlineArgs::Create();
        assert(canUnload() == S_FALSE);
        assert(args->ExitCode() == 0);
        assert(args->CurrentDirectory().empty());
        assert(args->CurrentEnvironment().empty());
        assert(args->ShowWindowCommand() == SW_NORMAL);

        const wchar_t environment[] = L"ONE=1\0TWO=2\0";
        const winrt::hstring env{ environment, static_cast<uint32_t>(std::size(environment) - 1) };
        args->CurrentDirectory(L"C:\\a path with spaces");
        args->CurrentEnvironment(env);
        args->ShowWindowCommand(SW_HIDE);
        auto alias = args;
        assert(alias->CurrentDirectory() == L"C:\\a path with spaces");
        assert(alias->CurrentEnvironment() == env);
        assert(alias->CurrentEnvironment().size() == 12);
        assert(alias->ShowWindowCommand() == SW_HIDE);

        const winrt::hstring command[]{ L"wt.exe", L"-w", L"native-abi-test", L"new-tab", L"--title", L"A spaced title", L"cmd.exe" };
        args->Commandline(command);
        assert(args->ExitCode() == 0);
        assert(args->TargetWindow() == L"native-abi-test");
        auto copy = args->Commandline();
        copy[0] = L"changed copy";
        assert(args->CommandlineRef()[0] == L"wt.exe");
        args->CommandlineRef()[0] = L"changed original";
        assert(alias->Commandline()[0] == L"changed original");
        assert(alias->TargetWindow() == L"native-abi-test");

        TestProjectedCommandlineBridge(args);
        auto weak = args->get_weak();
        args = nullptr;
        assert(weak.get().get() == alias.get());
        alias = nullptr;
        assert(!weak.get());
        assert(canUnload() == S_FALSE);
        weak = {};
        assert(canUnload() == S_OK);

        auto invalid = CommandlineArgs::Create();
        const winrt::hstring badCommand[]{ L"wt.exe", L"--no-such-native-test-option" };
        invalid->Commandline(badCommand);
        assert(invalid->ExitCode() != 0);
        assert(!invalid->ExitMessage().empty());
    }
    assert(canUnload() == S_OK);
    winrt::uninit_apartment();
}
