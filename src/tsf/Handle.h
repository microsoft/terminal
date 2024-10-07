// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::Render
{
    class Renderer;
}

namespace Microsoft::Console::TSF
{
    struct Implementation;

    // It is fine for any of the IDataProvider functions to throw.
    // However, this doesn't apply to the IUnknown ones.
    //
    // NOTE: This is a pure virtual interface, just like those for COM.
    // It cannot have a `~IDataProvider() = default;` destructor, because then it would need a vtable.
    MIDL_INTERFACE("A86B8AAF-1531-40F5-95BB-611AA9DBDC18")
    IDataProvider : IUnknown
    {
        virtual HWND GetHwnd() = 0;
        virtual RECT GetViewport() = 0;
        virtual RECT GetCursorPosition() = 0;
        virtual void HandleOutput(std::wstring_view text) = 0;
        virtual Render::Renderer* GetRenderer() = 0;
    };

    // A pimpl idiom wrapper for `Implementation` so that we don't pull in all the TSF headers everywhere.
    // Simultaneously it allows us to handle AdviseSink/UnadviseSink properly, because those hold strong
    // references on `Implementation` which results in an (unfortunate but intentional) reference cycle.
    struct Handle
    {
        static Handle Create();
        static void SetDefaultScopeAlphanumericHalfWidth(bool enable);

        Handle() = default;
        ~Handle();
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle(Handle&& other) noexcept;
        Handle& operator=(Handle&& other) noexcept;

        explicit operator bool() const noexcept;
        HWND FindWindowOfActiveTSF() const noexcept;
        void AssociateFocus(IDataProvider* provider) const;
        void Focus(IDataProvider* provider) const;
        void Unfocus(IDataProvider* provider) const;
        bool HasActiveComposition() const noexcept;

    private:
        Implementation* _impl = nullptr;
    };
}
