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

    Windows::UI::Xaml::UIElement TitlebarControl::Content()
    {
        return ContentRoot().Children().Size() > 0 ? ContentRoot().Children().GetAt(0) : nullptr;
    }

    void TitlebarControl::Content(Windows::UI::Xaml::UIElement content)
    {
        ContentRoot().Children().Clear();
        ContentRoot().Children().Append(content);
    }

    void TitlebarControl::Root_SizeChanged(const IInspectable& sender,
                                           const Windows::UI::Xaml::SizeChangedEventArgs& e)
    {
        const auto windowWidth = ActualWidth();
        const auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
        const auto dragBarMinWidth = DragBar().MinWidth();
        const auto maxWidth = windowWidth - minMaxCloseWidth - dragBarMinWidth;
        // Only set our MaxWidth if it's greater than 0. Setting it to a
        // negative value will cause a crash.
        if (maxWidth >= 0)
        {
            ContentRoot().MaxWidth(maxWidth);
        }
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
            MinMaxCloseControl().Maximize();
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MAXIMIZE | flag, lParam);
        }
        else if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            MinMaxCloseControl().RestoreDown();
            ::PostMessage(_window, WM_SYSCOMMAND, SC_RESTORE | flag, lParam);
        }
    }

    void TitlebarControl::Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        _OnMaximizeOrRestore(HTMAXBUTTON);
    }

    void TitlebarControl::DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e)
    {
        _OnMaximizeOrRestore(HTCAPTION);
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
        ::PostQuitMessage(0);
    }

}
