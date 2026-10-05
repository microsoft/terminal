// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <memory>
#include <unknwn.h>
#include <winrt/base.h>

#include "Api.h"

namespace TerminalApp
{
    class AppCommandlineArgs;
}

namespace TerminalApp::Native
{
    struct CommandlineArgsInterop;

    class CommandlineArgs : public winrt::implements<CommandlineArgs, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class CommandlineArgs;
        };

    public:
        APP_NATIVE_API HRESULT __stdcall QueryInterface(REFIID iid, void** object) noexcept;
        APP_NATIVE_API ULONG __stdcall AddRef() noexcept;
        APP_NATIVE_API ULONG __stdcall Release() noexcept;
        APP_NATIVE_API winrt::com_ptr<CommandlineArgs> get_strong() noexcept;
        APP_NATIVE_API winrt::weak_ref<CommandlineArgs> get_weak();
        APP_NATIVE_API static winrt::com_ptr<CommandlineArgs> Create();
        APP_NATIVE_API ~CommandlineArgs() noexcept;

        CommandlineArgs(ConstructionToken);

        APP_NATIVE_API int32_t ExitCode() const noexcept;
        APP_NATIVE_API winrt::hstring ExitMessage() const;
        APP_NATIVE_API winrt::hstring TargetWindow() const;
        APP_NATIVE_API void Commandline(winrt::array_view<const winrt::hstring> value);
        APP_NATIVE_API winrt::com_array<winrt::hstring> Commandline() const;
        APP_NATIVE_API winrt::com_array<winrt::hstring>& CommandlineRef() noexcept;
        APP_NATIVE_API winrt::hstring CurrentDirectory() const;
        APP_NATIVE_API void CurrentDirectory(const winrt::hstring& value);
        APP_NATIVE_API winrt::hstring CurrentEnvironment() const;
        APP_NATIVE_API void CurrentEnvironment(const winrt::hstring& value);
        APP_NATIVE_API uint32_t ShowWindowCommand() const noexcept;
        APP_NATIVE_API void ShowWindowCommand(uint32_t value) noexcept;

        APP_NATIVE_API AppCommandlineArgs& ParsedArgs() noexcept;

    private:
        using base_type = winrt::implements<CommandlineArgs, winrt::Windows::Foundation::IInspectable>;
        struct State;
        std::unique_ptr<State> _state;
        friend struct CommandlineArgsInterop;
    };

    using CommandlineArgsRef = winrt::com_ptr<CommandlineArgs>;
}
