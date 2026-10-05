// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../RemotingInterop.h"
#include "../NativeWindowRequestedArgsState.h"
#include "../Remoting.h"

namespace TerminalApp::Native
{
    WindowRequestedArgsRef WindowRequestedArgsInterop::FromProjected(const winrt::TerminalApp::WindowRequestedArgs& value)
    {
        if (!value) { return nullptr; }
        auto native = winrt::get_self<winrt::TerminalApp::implementation::WindowRequestedArgs>(value)->NativeArgs();
        const std::lock_guard lock{ native->_state->adapterMutex };
        if (!native->_state->adapter.get())
        {
            native->_state->adapter = winrt::make_weak(value.as<winrt::Windows::Foundation::IInspectable>());
        }
        return native;
    }

    winrt::TerminalApp::WindowRequestedArgs WindowRequestedArgsInterop::ToProjected(const WindowRequestedArgsRef& value)
    {
        if (!value) { return nullptr; }
        const std::lock_guard lock{ value->_state->adapterMutex };
        if (auto existing = value->_state->adapter.get())
        {
            return existing.as<winrt::TerminalApp::WindowRequestedArgs>();
        }
        auto adapter = winrt::make<winrt::TerminalApp::implementation::WindowRequestedArgs>(value);
        value->_state->adapter = winrt::make_weak(adapter.as<winrt::Windows::Foundation::IInspectable>());
        return adapter;
    }

    WindowRequestedArgsRef WindowRequestedArgsInterop::FromCommand(const uint64_t id, const winrt::TerminalApp::CommandlineArgs& command)
    {
        auto native = WindowRequestedArgs::Create(id, CommandlineArgsInterop::FromProjected(command));
        native->_state->commandAdapter = command;
        return native;
    }

    WindowRequestedArgsRef WindowRequestedArgsInterop::FromContent(
        const winrt::hstring& window, const winrt::hstring& content,
        const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect>& bounds)
    {
        std::optional<WindowBounds> nativeBounds;
        if (bounds)
        {
            const auto rect = bounds.Value();
            nativeBounds = WindowBounds{ rect.X, rect.Y, rect.Width, rect.Height };
        }
        auto native = WindowRequestedArgs::Create(window, content, nativeBounds);
        native->_state->boundsAdapter = bounds;
        return native;
    }

    winrt::TerminalApp::CommandlineArgs WindowRequestedArgsInterop::Command(const WindowRequestedArgsRef& value)
    {
        const std::lock_guard lock{ value->_state->adapterMutex };
        if (!value->_state->commandAdapter && value->_state->command)
        {
            value->_state->commandAdapter = CommandlineArgsInterop::ToProjected(value->_state->command);
        }
        return value->_state->commandAdapter ?
                   value->_state->commandAdapter.as<winrt::TerminalApp::CommandlineArgs>() :
                   nullptr;
    }

    winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect> WindowRequestedArgsInterop::InitialBounds(const WindowRequestedArgsRef& value)
    {
        const std::lock_guard lock{ value->_state->adapterMutex };
        if (!value->_state->boundsAdapter && value->_state->bounds)
        {
            const auto& rect = *value->_state->bounds;
            value->_state->boundsAdapter = winrt::box_value(winrt::Windows::Foundation::Rect{ rect.X, rect.Y, rect.Width, rect.Height })
                                              .as<winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect>>();
        }
        return value->_state->boundsAdapter;
    }

    winrt::Microsoft::Terminal::Settings::Model::WindowLayout WindowRequestedArgsInterop::PersistedLayout(const WindowRequestedArgsRef& value)
    {
        return value->_state->persistedLayout;
    }

    void WindowRequestedArgsInterop::PersistedLayout(const WindowRequestedArgsRef& value, const winrt::Microsoft::Terminal::Settings::Model::WindowLayout& layout)
    {
        value->_state->persistedLayout = layout;
    }

    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> WindowRequestedArgsInterop::StartupActions(const WindowRequestedArgsRef& value)
    {
        return value->_state->startupActions;
    }

    void WindowRequestedArgsInterop::StartupActions(const WindowRequestedArgsRef& value, const winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions)
    {
        value->_state->startupActions = actions;
    }

    uint32_t WindowRequestedArgsInterop::ApplyStartup(const WindowRequestedArgsRef& value, const winrt::TerminalApp::TerminalWindow& window)
    {
        if (!value || !window)
        {
            LOG_HR(E_INVALIDARG);
            throw winrt::hresult_invalid_argument();
        }
        const auto& state = *value->_state;
        if (state.persistedLayout)
        {
            window.SetPersistedLayout(state.persistedLayout);
        }
        else if (!state.content.empty())
        {
            window.SetStartupContent(state.content, InitialBounds(value));
        }
        else if (state.startupActions && state.startupActions.Size() > 0)
        {
            window.SetStartupActions(state.startupActions);
        }
        else
        {
            if (!state.command)
            {
                LOG_HR(E_INVALIDARG);
                throw winrt::hresult_invalid_argument();
            }
            window.SetStartupCommandline(Command(value));
            return state.command->ShowWindowCommand();
        }
        return SW_NORMAL;
    }
}
