//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

#include "MinMaxCloseControl.g.cpp"

namespace winrt::TerminalApp::implementation
{
    MinMaxCloseControl::MinMaxCloseControl()
    {
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///MinMaxCloseControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }

    uint64_t MinMaxCloseControl::ParentWindowHandle() const
    {
        return reinterpret_cast<uint64_t>(_window.get());
    }

    void MinMaxCloseControl::ParentWindowHandle(uint64_t handle)
    {
        _window.reset(reinterpret_cast<HWND>(handle));
    }

    void MinMaxCloseControl::_OnMaximize(byte flag)
    {
        if (_window)
        {
            POINT point1 = {};
            ::GetCursorPos(&point1);
            const LPARAM lParam = MAKELPARAM(point1.x, point1.y);
            WINDOWPLACEMENT placement = { sizeof(placement) };
            ::GetWindowPlacement(_window.get(), &placement);
            if (placement.showCmd == SW_SHOWNORMAL)
            {
                winrt::Windows::UI::Xaml::VisualStateManager::GoToState(this->Maximize(), L"WindowStateMaximized", false);
                ::PostMessage(_window.get(), WM_SYSCOMMAND, SC_MAXIMIZE | flag, lParam);
            }
            else if (placement.showCmd == SW_SHOWMAXIMIZED)
            {
                winrt::Windows::UI::Xaml::VisualStateManager::GoToState(this->Maximize(), L"WindowStateNormal", false);
                ::PostMessage(_window.get(), WM_SYSCOMMAND, SC_RESTORE | flag, lParam);
            }
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
        if (_window)
        {
            ::PostMessage(_window.get(), WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
        }
    }

    void MinMaxCloseControl::Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ::PostQuitMessage(0);
    }

}
