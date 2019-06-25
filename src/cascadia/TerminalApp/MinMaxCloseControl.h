//
// Declaration of the MainUserControl class.
//

#pragma once

#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "MinMaxCloseControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl>
    {
        MinMaxCloseControl(uint64_t hWnd);

        void Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e);

    private:
        void _OnMaximize(byte flag);
        HWND _window = nullptr;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl, implementation::MinMaxCloseControl>
    {
    };
}
