// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "RemotingInterop.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>

#include <cassert>

using TerminalApp::Native::CommandlineArgsInterop;
using TerminalApp::Native::WindowRequestedArgsInterop;
using TerminalApp::Native::WindowRequestedArgsRef;
namespace Model = winrt::Microsoft::Terminal::Settings::Model;
namespace Foundation = winrt::Windows::Foundation;

void TestProjectedWindowRequestBridge(const WindowRequestedArgsRef& value)
{
    assert(!WindowRequestedArgsInterop::FromProjected(nullptr));
    assert(!WindowRequestedArgsInterop::ToProjected(nullptr));
    bool rejectedNull = false;
    try
    {
        WindowRequestedArgsInterop::ApplyStartup(nullptr, nullptr);
    }
    catch (const winrt::hresult_error& error)
    {
        rejectedNull = error.code() == E_INVALIDARG;
    }
    assert(rejectedNull);
    {
        const auto first = WindowRequestedArgsInterop::ToProjected(value);
        const auto second = WindowRequestedArgsInterop::ToProjected(value);
        assert(first == second);
        assert(WindowRequestedArgsInterop::FromProjected(first).get() == value.get());
        first.Id(81);
        first.WindowName(L"changed through compatibility");
        assert(value->Id() == 81 && value->WindowName() == L"changed through compatibility");
        assert(CommandlineArgsInterop::FromProjected(first.Command()).get() == value->Command().get());

        auto actions = winrt::single_threaded_vector<Model::ActionAndArgs>();
        first.StartupActions(actions);
        assert(WindowRequestedArgsInterop::StartupActions(value) == actions);
        actions.Append(Model::ActionAndArgs{});
        assert(second.StartupActions().Size() == 1);
        Model::WindowLayout layout;
        layout.TabLayout(actions);
        WindowRequestedArgsInterop::PersistedLayout(value, layout);
        assert(first.PersistedLayout() == layout);
        assert(first.PersistedLayout().TabLayout() == actions);

        const auto bounds = winrt::box_value(Foundation::Rect{ -4, 6, 0, 0 }).as<Foundation::IReference<Foundation::Rect>>();
        winrt::TerminalApp::WindowRequestedArgs legacy{ L"legacy", L"{}", bounds };
        const auto native = WindowRequestedArgsInterop::FromProjected(legacy);
        assert(WindowRequestedArgsInterop::ToProjected(native) == legacy);
        assert(WindowRequestedArgsInterop::InitialBounds(native) == bounds);
        assert(native->InitialBounds()->X == -4);
        assert(!legacy.Command());

        winrt::TerminalApp::CommandlineArgs command;
        const auto weakCommand = winrt::make_weak(command);
        winrt::TerminalApp::WindowRequestedArgs commandRequest{ 7, command };
        const auto nativeRequest = WindowRequestedArgsInterop::FromProjected(commandRequest);
        command = nullptr;
        assert(weakCommand.get());
        assert(WindowRequestedArgsInterop::Command(nativeRequest) == commandRequest.Command());
    }
    winrt::clear_factory_cache();
}
