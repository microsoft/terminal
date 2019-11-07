#include "pch.h"

#include "Generated Files\winrt\TerminalApp.h"

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment();
    ::winrt::Windows::UI::Xaml::Application::Start(
        [](auto&&) {
            ::winrt::TerminalApp::App a;
            ::winrt::detach_abi(a);
        });
    return 0;
}
