//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

namespace winrt::TerminalApp::implementation
{
    MinMaxCloseControl::MinMaxCloseControl(uint64_t hWnd)
        : _window(reinterpret_cast<HWND>(hWnd))
    {
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///MinMaxCloseControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }
}

#include "MinMaxCloseControl.g.cpp"

void winrt::TerminalApp::implementation::MinMaxCloseControl::OnMaximize(byte flag)
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

void winrt::TerminalApp::implementation::MinMaxCloseControl::Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
{
    OnMaximize(HTMAXBUTTON);
}

void winrt::TerminalApp::implementation::MinMaxCloseControl::DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e)
{
    OnMaximize(HTCAPTION);
}

void winrt::TerminalApp::implementation::MinMaxCloseControl::Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
{
    ::PostMessage(_window, WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
}

void winrt::TerminalApp::implementation::MinMaxCloseControl::Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
{
    ::PostMessage(_window, WM_SYSCOMMAND, SC_CLOSE | HTCLOSE, 0);
}
