//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

#include "MinMaxCloseControl.g.cpp"

namespace winrt::TerminalApp::implementation
{
    MinMaxCloseControl::MinMaxCloseControl(uint64_t hWnd) :
        _window(reinterpret_cast<HWND>(hWnd))
    {
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///MinMaxCloseControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }

    void MinMaxCloseControl::_OnMaximize(byte flag)
    {
        POINT point1 = {};
        ::GetCursorPos(&point1);
        const LPARAM lParam = MAKELPARAM(point1.x, point1.y);
        WINDOWPLACEMENT placement = { sizeof(placement) };
        ::GetWindowPlacement(_window, &placement);
        if (placement.showCmd == SW_SHOWNORMAL)
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(this->Maximize(), L"WindowStateMaximized", false);
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MAXIMIZE | flag, lParam);
        }
        else if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(this->Maximize(), L"WindowStateNormal", false);
            ::PostMessage(_window, WM_SYSCOMMAND, SC_RESTORE | flag, lParam);
        }
    }

    void MinMaxCloseControl::Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        _OnMaximize(HTMAXBUTTON);
    }

    void MinMaxCloseControl::DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e)
    {
        _OnMaximize(HTCAPTION);
    }

    void MinMaxCloseControl::Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ::PostMessage(_window, WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
    }

    void MinMaxCloseControl::Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ::PostQuitMessage(0);
    }

}
