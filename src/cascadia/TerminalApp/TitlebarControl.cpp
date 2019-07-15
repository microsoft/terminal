//
// TitlebarControl.xaml.cpp
// Implementation of the TitlebarControl class
//

#include "pch.h"

#include "TitlebarControl.h"

#include "TitlebarControl.g.cpp"

namespace winrt::TerminalApp::implementation
{
    TitlebarControl::TitlebarControl()
    {
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///TitlebarControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);

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

    void TitlebarControl::Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        const auto windowWidth = ActualWidth();
        const auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
        const auto dragBarMinWidth = DragBar().MinWidth();
        const auto maxWidth = windowWidth - minMaxCloseWidth - dragBarMinWidth;
        ContentRoot().MaxWidth(maxWidth);
    }

    uint64_t TitlebarControl::ParentWindowHandle() const
    {
        return reinterpret_cast<uint64_t>(_window);
    }

    void TitlebarControl::ParentWindowHandle(uint64_t handle)
    {
        _window = reinterpret_cast<HWND>(handle);
    }

    void TitlebarControl::_OnMaximizeOrRestore(byte flag)
    {
        if (_window)
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
