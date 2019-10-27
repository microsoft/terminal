// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// TitlebarControl.xaml.cpp
// Implementation of the TitlebarControl class
//

#include "pch.h"

#include "TitlebarControl.h"

#include "TitlebarControl.g.cpp"

namespace winrt::TerminalApp::implementation
{
    TitlebarControl::TitlebarControl(uint64_t handle) :
        _window{ reinterpret_cast<HWND>(handle) }
    {
        InitializeComponent();

        // Register our event handlers on the MMC buttons.
        MinMaxCloseControl().MinimizeClick({ this, &TitlebarControl::Minimize_Click });
        MinMaxCloseControl().MaximizeClick({ this, &TitlebarControl::Maximize_Click });
        MinMaxCloseControl().CloseClick({ this, &TitlebarControl::Close_Click });
    }

    IInspectable TitlebarControl::Content()
    {
        return ContentRoot().Content();
    }

    void TitlebarControl::Content(IInspectable content)
    {
        ContentRoot().Content(content);
    }

    void TitlebarControl::_OnMaximizeOrRestore(byte flag)
    {
        POINT point1 = {};
        ::GetCursorPos(&point1);
        const LPARAM lParam = MAKELPARAM(point1.x, point1.y);
        WINDOWPLACEMENT placement = { sizeof(placement) };
        ::GetWindowPlacement(_window, &placement);
        if (placement.showCmd == SW_SHOWNORMAL)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MAXIMIZE | flag, lParam);
        }
        else if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_RESTORE | flag, lParam);
        }
    }

    void TitlebarControl::Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        _OnMaximizeOrRestore(HTMAXBUTTON);
    }

    void TitlebarControl::Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        if (_window)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
        }
    }

    void TitlebarControl::Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ::PostMessage(_window, WM_SYSCOMMAND, SC_CLOSE, 0);
    }

    void TitlebarControl::SetWindowVisualState(WindowVisualState visualState)
    {
        MinMaxCloseControl().SetWindowVisualState(visualState);
    }

}
