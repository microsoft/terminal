// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/CommandlineArgs.h"
#include "Native/WindowRequestedArgs.h"
#include "CommandlineArgs.g.h"
#include "RequestReceiveContentArgs.g.h"
#include "WindowRequestedArgs.g.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandlineArgs : public CommandlineArgsT<CommandlineArgs>
    {
        CommandlineArgs() = default;
        explicit CommandlineArgs(::TerminalApp::Native::CommandlineArgsRef value) :
            _native{ std::move(value) }
        {
        }

        ::TerminalApp::Native::CommandlineArgsRef NativeArgs() const noexcept { return _native; }
        ::TerminalApp::AppCommandlineArgs& ParsedArgs() noexcept;
        winrt::com_array<winrt::hstring>& CommandlineRef() noexcept;

        int32_t ExitCode() const noexcept;
        winrt::hstring ExitMessage() const;
        winrt::hstring TargetWindow() const;

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection Connection() const;
        void Connection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& value);
        void Commandline(const winrt::array_view<const winrt::hstring>& value);
        winrt::com_array<winrt::hstring> Commandline();
        winrt::hstring CurrentDirectory() const;
        void CurrentDirectory(const winrt::hstring& value);
        winrt::hstring CurrentEnvironment() const;
        void CurrentEnvironment(const winrt::hstring& value);
        uint32_t ShowWindowCommand() const noexcept;
        void ShowWindowCommand(uint32_t value) noexcept;

    private:
        ::TerminalApp::Native::CommandlineArgsRef _native{ ::TerminalApp::Native::CommandlineArgs::Create() };
    };

    struct RequestReceiveContentArgs : RequestReceiveContentArgsT<RequestReceiveContentArgs>
    {
        WINRT_PROPERTY(uint64_t, SourceWindow);
        WINRT_PROPERTY(uint64_t, TargetWindow);
        WINRT_PROPERTY(uint32_t, TabIndex);

    public:
        RequestReceiveContentArgs(const uint64_t src, const uint64_t tgt, const uint32_t tabIndex) :
            _SourceWindow{ src },
            _TargetWindow{ tgt },
            _TabIndex{ tabIndex } {};
    };

    struct WindowRequestedArgs : public WindowRequestedArgsT<WindowRequestedArgs>
    {
    public:
        WindowRequestedArgs(uint64_t id, const winrt::TerminalApp::CommandlineArgs& command);
        WindowRequestedArgs(const winrt::hstring& window, const winrt::hstring& content, const Windows::Foundation::IReference<Windows::Foundation::Rect>& bounds);
        explicit WindowRequestedArgs(::TerminalApp::Native::WindowRequestedArgsRef value) :
            _native{ std::move(value) }
        {
        }

        ::TerminalApp::Native::WindowRequestedArgsRef NativeArgs() const noexcept { return _native; }
        uint64_t Id() const noexcept;
        void Id(uint64_t value) noexcept;
        winrt::hstring WindowName() const;
        void WindowName(const winrt::hstring& value);
        TerminalApp::CommandlineArgs Command() const;
        winrt::hstring Content() const;
        Windows::Foundation::IReference<Windows::Foundation::Rect> InitialBounds() const;
        winrt::Microsoft::Terminal::Settings::Model::WindowLayout PersistedLayout() const;
        void PersistedLayout(const winrt::Microsoft::Terminal::Settings::Model::WindowLayout& value);
        Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> StartupActions() const;
        void StartupActions(const Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& value);

    private:
        ::TerminalApp::Native::WindowRequestedArgsRef _native;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandlineArgs);
    BASIC_FACTORY(RequestReceiveContentArgs);
    BASIC_FACTORY(WindowRequestedArgs);
}
