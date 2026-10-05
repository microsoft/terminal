// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "WindowRequestedArgs.h"
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include "../NativeWindowRequestedArgsState.h"

namespace TerminalApp::Native
{
    HRESULT __stdcall WindowRequestedArgs::QueryInterface(REFIID iid, void** object) noexcept
    {
        return base_type::QueryInterface(iid, object);
    }

    ULONG __stdcall WindowRequestedArgs::AddRef() noexcept { return base_type::AddRef(); }
    ULONG __stdcall WindowRequestedArgs::Release() noexcept { return base_type::Release(); }
    winrt::com_ptr<WindowRequestedArgs> WindowRequestedArgs::get_strong() noexcept { return base_type::get_strong(); }
    winrt::weak_ref<WindowRequestedArgs> WindowRequestedArgs::get_weak() { return base_type::get_weak(); }

    WindowRequestedArgs::WindowRequestedArgs(ConstructionToken) :
        _state{ std::make_unique<State>() }
    {
    }

    WindowRequestedArgs::~WindowRequestedArgs() noexcept = default;

    winrt::com_ptr<WindowRequestedArgs> WindowRequestedArgs::Create(const uint64_t id, const CommandlineArgsRef& command)
    {
        auto result = winrt::make_self<WindowRequestedArgs>(ConstructionToken{});
        result->_state->id = id;
        result->_state->command = command;
        return result;
    }

    winrt::com_ptr<WindowRequestedArgs> WindowRequestedArgs::Create(
        const winrt::hstring& window, const winrt::hstring& content, const std::optional<WindowBounds>& bounds)
    {
        auto result = winrt::make_self<WindowRequestedArgs>(ConstructionToken{});
        result->_state->windowName = window;
        result->_state->content = content;
        result->_state->bounds = bounds;
        return result;
    }

    uint64_t WindowRequestedArgs::Id() const noexcept { return _state->id; }
    void WindowRequestedArgs::Id(const uint64_t value) noexcept { _state->id = value; }
    winrt::hstring WindowRequestedArgs::WindowName() const { return _state->windowName; }
    void WindowRequestedArgs::WindowName(const winrt::hstring& value) { _state->windowName = value; }
    CommandlineArgsRef WindowRequestedArgs::Command() const noexcept { return _state->command; }
    winrt::hstring WindowRequestedArgs::Content() const { return _state->content; }
    std::optional<WindowBounds> WindowRequestedArgs::InitialBounds() const noexcept { return _state->bounds; }
}
