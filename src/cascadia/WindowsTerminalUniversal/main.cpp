#include "pch.h"

#include "Generated Files\winrt\TerminalApp.h"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    ::winrt::Windows::UI::Xaml::Application::Start(
        [](auto&&) {
            ::winrt::TerminalApp::App a;

            // We dump the pointer into nowhere here because it needs to not be deconstructed
            // upon leaving the bottom of this lambda and the XAML Framework will catch the
            // appropriate reference into App.Current for the remainder of execution.
            ::winrt::detach_abi(a);
        });
    return 0;
}
