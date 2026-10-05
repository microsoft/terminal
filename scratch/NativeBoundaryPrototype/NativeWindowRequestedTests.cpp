// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "WindowRequestedArgs.h"

#include <cassert>
#include <windows.h>

using TerminalApp::Native::CommandlineArgs;
using TerminalApp::Native::WindowBounds;
using TerminalApp::Native::WindowRequestedArgs;
using TerminalApp::Native::WindowRequestedArgsRef;

void TestProjectedWindowRequestBridge(const WindowRequestedArgsRef& value);

int main()
{
    winrt::init_apartment();
    const auto canUnload = reinterpret_cast<HRESULT(WINAPI*)()>(
        GetProcAddress(GetModuleHandleW(L"TerminalApp.dll"), "DllCanUnloadNow"));
    assert(canUnload && canUnload() == S_OK);
    {
        auto command = CommandlineArgs::Create();
        auto request = WindowRequestedArgs::Create(0, command);
        auto weakCommand = command->get_weak();
        assert(request->Id() == 0);
        assert(request->WindowName().empty());
        assert(request->Content().empty());
        assert(!request->InitialBounds());
        assert(request->Command().get() == command.get());
        auto alias = request;
        request->Id(42);
        request->WindowName(L"Native request");
        assert(alias->Id() == 42 && alias->WindowName() == L"Native request");
        command = nullptr;
        assert(weakCommand.get().get() == request->Command().get());

        TestProjectedWindowRequestBridge(request);
        auto weak = request->get_weak();
        request = nullptr;
        assert(weak.get().get() == alias.get());
        alias = nullptr;
        assert(!weak.get() && !weakCommand.get());
        assert(canUnload() == S_FALSE);
        weak = {};
        weakCommand = {};
        assert(canUnload() == S_OK);
    }
    {
        auto empty = WindowRequestedArgs::Create(L"empty", L"{}", std::nullopt);
        auto zero = WindowRequestedArgs::Create(L"zero", L"{}", WindowBounds{ 0, 0, 0, 0 });
        const WindowBounds rect{ -10, 20, 300.5f, 400.25f };
        auto positioned = WindowRequestedArgs::Create(L"positioned", L"{\"tabs\":[]}", rect);
        assert(!empty->InitialBounds());
        assert(zero->InitialBounds().has_value());
        assert((zero->InitialBounds().value() == WindowBounds{ 0, 0, 0, 0 }));
        assert(positioned->InitialBounds().value() == rect);
        assert(positioned->Content() == L"{\"tabs\":[]}");
        assert(!positioned->Command());
    }
    assert(canUnload() == S_OK);
    winrt::uninit_apartment();
}
