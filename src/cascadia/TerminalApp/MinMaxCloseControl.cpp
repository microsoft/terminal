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

    void MinMaxCloseControl::Maximize()
    {
        winrt::Windows::UI::Xaml::VisualStateManager::GoToState(MaximizeButton(), L"WindowStateMaximized", false);
    }

    void MinMaxCloseControl::RestoreDown()
    {
        winrt::Windows::UI::Xaml::VisualStateManager::GoToState(MaximizeButton(), L"WindowStateNormal", false);
    }

    // These need to be defined here to make the compiler happy. They do nothing
    // on their own, it's up to the control that's embedding us to set a
    // callback when these buttons are pressed.
    void MinMaxCloseControl::Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        _minimizeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
    }
    void MinMaxCloseControl::Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, MinimizeClick, _minimizeClickHandlers, TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs);

}
