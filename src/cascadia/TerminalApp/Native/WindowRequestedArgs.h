// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CommandlineArgs.h"

#include <optional>

namespace TerminalApp::Native
{
    struct WindowBounds
    {
        float X;
        float Y;
        float Width;
        float Height;
        bool operator==(const WindowBounds&) const = default;
    };

    struct WindowRequestedArgsInterop;

    class WindowRequestedArgs : public winrt::implements<WindowRequestedArgs, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class WindowRequestedArgs;
        };

    public:
        APP_NATIVE_API HRESULT __stdcall QueryInterface(REFIID iid, void** object) noexcept;
        APP_NATIVE_API ULONG __stdcall AddRef() noexcept;
        APP_NATIVE_API ULONG __stdcall Release() noexcept;
        APP_NATIVE_API winrt::com_ptr<WindowRequestedArgs> get_strong() noexcept;
        APP_NATIVE_API winrt::weak_ref<WindowRequestedArgs> get_weak();
        APP_NATIVE_API static winrt::com_ptr<WindowRequestedArgs> Create(uint64_t id, const CommandlineArgsRef& command);
        APP_NATIVE_API static winrt::com_ptr<WindowRequestedArgs> Create(
            const winrt::hstring& window, const winrt::hstring& content, const std::optional<WindowBounds>& bounds);
        APP_NATIVE_API ~WindowRequestedArgs() noexcept;

        WindowRequestedArgs(ConstructionToken);

        APP_NATIVE_API uint64_t Id() const noexcept;
        APP_NATIVE_API void Id(uint64_t value) noexcept;
        APP_NATIVE_API winrt::hstring WindowName() const;
        APP_NATIVE_API void WindowName(const winrt::hstring& value);
        APP_NATIVE_API CommandlineArgsRef Command() const noexcept;
        APP_NATIVE_API winrt::hstring Content() const;
        APP_NATIVE_API std::optional<WindowBounds> InitialBounds() const noexcept;

    private:
        using base_type = winrt::implements<WindowRequestedArgs, winrt::Windows::Foundation::IInspectable>;
        struct State;
        std::unique_ptr<State> _state;
        friend struct WindowRequestedArgsInterop;
    };

    using WindowRequestedArgsRef = winrt::com_ptr<WindowRequestedArgs>;
}
