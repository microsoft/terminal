#include "pch.h"
#include "TerminalPage.h"
#if __has_include("TerminalPage.g.cpp")
#include "TerminalPage.g.cpp"
#endif

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage()
    {
        // Xaml will by default attempt to load from ms-appx://TerminalApp/TerminalPage.xaml.
        // We'll force it to load from the root of the appx instead.
        const winrt::Windows::Foundation::Uri resourceLocator{ L"ms-appx:///TerminalPage.xaml" };
        winrt::Windows::UI::Xaml::Application::LoadComponent(*this, resourceLocator, winrt::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Nested);
    }

    // Method Description:
    // - Bound in the Xaml editor to the [+] button.
    // Arguments:
    // - sender
    // - event arguments
    void TerminalPage::OnNewTabButtonClick(IInspectable const&, Controls::SplitButtonClickEventArgs const&)
    {
    }
}
