// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TmuxConnection.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage;

    struct __declspec(uuid("ed2ef5e8-b934-49a4-8727-cd3590836bf4")) ITmuxWindowIdentity : ::IUnknown
    {
    };

    // An in-process startup argument and owner for one session's native window.
    struct TmuxWindow : winrt::implements<TmuxWindow, Microsoft::Terminal::Settings::Model::INewContentArgs, winrt::cloaked<ITmuxWindowIdentity>>
    {
        TmuxWindow(TerminalPage& source,
                   const Microsoft::Terminal::Settings::Model::Profile& profile,
                   std::shared_ptr<TmuxSession> session);
        ~TmuxWindow();

        static Microsoft::Terminal::TerminalConnection::ITerminalConnection Wrap(
            TerminalPage& source,
            const Microsoft::Terminal::TerminalConnection::ITerminalConnection& transport,
            const Microsoft::Terminal::Settings::Model::Profile& profile,
            uint32_t historyLimit);
        static winrt::com_ptr<TmuxWindow> From(const Microsoft::Terminal::Settings::Model::INewContentArgs& args);

        void Attach(TerminalPage& page);
        bool TryNewTab(const Microsoft::Terminal::Settings::Model::INewContentArgs& args);
        void NewTab();
        void Detach();
        void UpdateControls();

        hstring Type() const { return L"tmux"; }
        bool Equals(const Microsoft::Terminal::Settings::Model::INewContentArgs& other) const;
        uint64_t Hash() const noexcept { return reinterpret_cast<uintptr_t>(this); }
        Microsoft::Terminal::Settings::Model::INewContentArgs Copy() const { return *this; }
        hstring GenerateName() const { return _profile.Name(); }
        hstring GenerateName(const Windows::ApplicationModel::Resources::Core::ResourceContext&) const { return GenerateName(); }

    private:
        std::shared_ptr<TmuxSession> _session;
        Microsoft::Terminal::Settings::Model::Profile _profile;
        struct State;
        std::unique_ptr<State> _state;
        Windows::UI::Xaml::FrameworkElement::Unloaded_revoker _unloaded;
        Windows::UI::Xaml::Controls::Button _newTabButton{ nullptr };
        std::vector<::Microsoft::Terminal::Tmux::Window> _windows;
        bool _requested{};
        bool _detaching{};

        winrt::com_ptr<TerminalPage> _Page() const;
        void _WindowsChanged(std::vector<::Microsoft::Terminal::Tmux::Window> windows);
    };
}
