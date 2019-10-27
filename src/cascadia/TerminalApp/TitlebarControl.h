// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Declaration of the MainUserControl class.
//

#pragma once

#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "TitlebarControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TitlebarControl : TitlebarControlT<TitlebarControl>
    {
        TitlebarControl(uint64_t handle);

        IInspectable Content();
        void Content(IInspectable content);

        void SetWindowVisualState(WindowVisualState visualState);

        void Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        void _OnMaximizeOrRestore(byte flag);
        HWND _window{ nullptr }; // non-owning handle; should not be freed in the dtor.
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TitlebarControl : TitlebarControlT<TitlebarControl, implementation::TitlebarControl>
    {
    };
}
