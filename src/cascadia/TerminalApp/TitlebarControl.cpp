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
    }

    uint64_t TitlebarControl::ParentWindowHandle()
    {
        return MinMaxCloseControl().ParentWindowHandle();
    }

    void TitlebarControl::ParentWindowHandle(uint64_t handle)
    {
        MinMaxCloseControl().ParentWindowHandle(handle);
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

    void TitlebarControl::DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e)
    {
        MinMaxCloseControl().DoubleClickDragBar();
    }

    void TitlebarControl::Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        const auto windowWidth = ActualWidth();
        const auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
        const auto dragBarMinWidth = DragBar().MinWidth();
        const auto maxWidth = windowWidth - minMaxCloseWidth - dragBarMinWidth;
        ContentRoot().MaxWidth(maxWidth);
    }
}
