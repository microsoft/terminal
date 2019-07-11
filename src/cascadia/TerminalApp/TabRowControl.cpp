// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabRowControl.h"

#include "TabRowControl.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TabRowControl::TabRowControl()
    {
        // The generated code will by default attempt to load from ms-appx://TerminalApp/TabRowControl.xaml.
        // We'll force it to load from the root of the appx instead.
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///TabRowControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }

    // Method Description:
    // - Bound in the Xaml editor to the [+] button.
    // Arguments:
    // - sender
    // - event arguments
    void TabRowControl::OnNewTabButtonClick(IInspectable const&, Controls::SplitButtonClickEventArgs const&)
    {
    }

    // void TabRowControl::Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    // {
    //     auto windowWidth = Root().ActualWidth();
    //     auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
    //     TabView().MaxWidth(windowWidth - minMaxCloseWidth);
    // }

}
