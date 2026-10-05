// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Remoting.h"
#include "RemotingInterop.h"

#include "CommandlineArgs.g.cpp"
#include "RequestReceiveContentArgs.g.cpp"
#include "WindowRequestedArgs.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;

namespace winrt::TerminalApp::implementation
{
    ::TerminalApp::AppCommandlineArgs& CommandlineArgs::ParsedArgs() noexcept
    {
        return _native->ParsedArgs();
    }

    winrt::com_array<winrt::hstring>& CommandlineArgs::CommandlineRef() noexcept
    {
        return _native->CommandlineRef();
    }

    int32_t CommandlineArgs::ExitCode() const noexcept
    {
        return _native->ExitCode();
    }

    winrt::hstring CommandlineArgs::ExitMessage() const
    {
        return _native->ExitMessage();
    }

    winrt::hstring CommandlineArgs::TargetWindow() const
    {
        return _native->TargetWindow();
    }

    void CommandlineArgs::Commandline(const winrt::array_view<const winrt::hstring>& value)
    {
        _native->Commandline(value);
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Commandline()
    {
        return _native->Commandline();
    }

    winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CommandlineArgs::Connection() const
    {
        return ::TerminalApp::Native::CommandlineArgsInterop::Connection(_native);
    }

    void CommandlineArgs::Connection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& value)
    {
        ::TerminalApp::Native::CommandlineArgsInterop::Connection(_native, value);
    }

    winrt::hstring CommandlineArgs::CurrentDirectory() const { return _native->CurrentDirectory(); }
    void CommandlineArgs::CurrentDirectory(const winrt::hstring& value) { _native->CurrentDirectory(value); }
    winrt::hstring CommandlineArgs::CurrentEnvironment() const { return _native->CurrentEnvironment(); }
    void CommandlineArgs::CurrentEnvironment(const winrt::hstring& value) { _native->CurrentEnvironment(value); }
    uint32_t CommandlineArgs::ShowWindowCommand() const noexcept { return _native->ShowWindowCommand(); }
    void CommandlineArgs::ShowWindowCommand(const uint32_t value) noexcept { _native->ShowWindowCommand(value); }

    WindowRequestedArgs::WindowRequestedArgs(const uint64_t id, const winrt::TerminalApp::CommandlineArgs& command) :
        _native{ ::TerminalApp::Native::WindowRequestedArgsInterop::FromCommand(id, command) }
    {
    }

    WindowRequestedArgs::WindowRequestedArgs(const winrt::hstring& window, const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& bounds) :
        _native{ ::TerminalApp::Native::WindowRequestedArgsInterop::FromContent(window, content, bounds) }
    {
    }

    uint64_t WindowRequestedArgs::Id() const noexcept { return _native->Id(); }
    void WindowRequestedArgs::Id(const uint64_t value) noexcept { _native->Id(value); }
    winrt::hstring WindowRequestedArgs::WindowName() const { return _native->WindowName(); }
    void WindowRequestedArgs::WindowName(const winrt::hstring& value) { _native->WindowName(value); }
    winrt::hstring WindowRequestedArgs::Content() const { return _native->Content(); }

    TerminalApp::CommandlineArgs WindowRequestedArgs::Command() const
    {
        return ::TerminalApp::Native::WindowRequestedArgsInterop::Command(_native);
    }

    Windows::Foundation::IReference<Windows::Foundation::Rect> WindowRequestedArgs::InitialBounds() const
    {
        return ::TerminalApp::Native::WindowRequestedArgsInterop::InitialBounds(_native);
    }

    winrt::Microsoft::Terminal::Settings::Model::WindowLayout WindowRequestedArgs::PersistedLayout() const
    {
        return ::TerminalApp::Native::WindowRequestedArgsInterop::PersistedLayout(_native);
    }

    void WindowRequestedArgs::PersistedLayout(const winrt::Microsoft::Terminal::Settings::Model::WindowLayout& value)
    {
        ::TerminalApp::Native::WindowRequestedArgsInterop::PersistedLayout(_native, value);
    }

    Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> WindowRequestedArgs::StartupActions() const
    {
        return ::TerminalApp::Native::WindowRequestedArgsInterop::StartupActions(_native);
    }

    void WindowRequestedArgs::StartupActions(const Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& value)
    {
        ::TerminalApp::Native::WindowRequestedArgsInterop::StartupActions(_native, value);
    }
}
