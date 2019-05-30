//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

namespace winrt::TerminalApp::implementation
{
    MinMaxCloseControl::MinMaxCloseControl()
    {
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///MinMaxCloseControl.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }
}

#include "MinMaxCloseControl.g.cpp"
