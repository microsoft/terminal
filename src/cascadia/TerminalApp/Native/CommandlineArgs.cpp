// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "CommandlineArgs.h"

#define BLOCK_TIL
#include <LibraryIncludes.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>
#include <CLI/CLI.hpp>
#include "til.h"

#include "../NativeCommandlineArgsState.h"

namespace TerminalApp::Native
{
    HRESULT __stdcall CommandlineArgs::QueryInterface(REFIID iid, void** object) noexcept
    {
        return base_type::QueryInterface(iid, object);
    }

    ULONG __stdcall CommandlineArgs::AddRef() noexcept { return base_type::AddRef(); }
    ULONG __stdcall CommandlineArgs::Release() noexcept { return base_type::Release(); }
    winrt::com_ptr<CommandlineArgs> CommandlineArgs::get_strong() noexcept { return base_type::get_strong(); }
    winrt::weak_ref<CommandlineArgs> CommandlineArgs::get_weak() { return base_type::get_weak(); }

    winrt::com_ptr<CommandlineArgs> CommandlineArgs::Create()
    {
        return winrt::make_self<CommandlineArgs>(ConstructionToken{});
    }

    CommandlineArgs::CommandlineArgs(ConstructionToken) :
        _state{ std::make_unique<State>() }
    {
    }

    CommandlineArgs::~CommandlineArgs() noexcept = default;

    int32_t CommandlineArgs::ExitCode() const noexcept { return _state->parseResult; }
    winrt::hstring CommandlineArgs::ExitMessage() const { return winrt::to_hstring(_state->parsed.GetExitMessage()); }
    winrt::hstring CommandlineArgs::TargetWindow() const { return winrt::to_hstring(_state->parsed.GetTargetWindow()); }

    void CommandlineArgs::Commandline(const winrt::array_view<const winrt::hstring> value)
    {
        _state->args = { value.begin(), value.end() };
        _state->parseResult = _state->parsed.ParseArgs(_state->args);
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Commandline() const
    {
        return { _state->args.begin(), _state->args.end() };
    }

    winrt::com_array<winrt::hstring>& CommandlineArgs::CommandlineRef() noexcept { return _state->args; }
    AppCommandlineArgs& CommandlineArgs::ParsedArgs() noexcept { return _state->parsed; }
    winrt::hstring CommandlineArgs::CurrentDirectory() const { return _state->currentDirectory; }
    void CommandlineArgs::CurrentDirectory(const winrt::hstring& value) { _state->currentDirectory = value; }
    winrt::hstring CommandlineArgs::CurrentEnvironment() const { return _state->currentEnvironment; }
    void CommandlineArgs::CurrentEnvironment(const winrt::hstring& value) { _state->currentEnvironment = value; }
    uint32_t CommandlineArgs::ShowWindowCommand() const noexcept { return _state->showWindowCommand; }
    void CommandlineArgs::ShowWindowCommand(const uint32_t value) noexcept { _state->showWindowCommand = value; }
}
