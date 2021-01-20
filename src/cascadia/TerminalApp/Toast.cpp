// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Toast.h"
#include <LibraryResources.h>
#include "Toast.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    Toast::Toast() :
        _root{},
        _dismissTimer{}
    {
        // Disable the "X" on the toast - the user can dismiss it by clicking anywhere
        _root.IsLightDismissEnabled(true);
        _root.IsTabStop(false);

        static constexpr auto DismissInterval = std::chrono::microseconds(static_cast<int>((3.0) * 1000000));
        _dismissTimer.Interval(DismissInterval);
        _dismissTimer.Tick({ get_weak(), &Toast::_onDismiss });
    };
    void Toast::Show()
    {
        _root.IsOpen(true);
        _dismissTimer.Start();
    }
    Windows::UI::Xaml::Controls::ContentControl Toast::Root()
    {
        return _root;
    }

    void Toast::_onDismiss(Windows::Foundation::IInspectable const& /* sender */,
                           Windows::Foundation::IInspectable const& /* e */)
    {
        // auto weakThis{ get_weak() };
        // co_await winrt::resume_foreground(_root.Dispatcher());
        // if (auto self{ weakThis.get() })
        // {
        _dismissTimer.Stop();
        // Root().Visibility(Visibility::Collapsed);
        _root.IsOpen(false);
        // }

        // TODO: this isn't working.
        // _ClosedHandlers(*this, nullptr);
    }
}
