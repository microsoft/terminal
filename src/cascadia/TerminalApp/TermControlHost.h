#pragma once
#include "TermControlHost.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>

namespace winrt::TerminalApp::implementation
{
    struct TermControlHost : TermControlHostT<TermControlHost>
    {
        TermControlHost(Microsoft::Terminal::TerminalControl::TermControl control);

        // Windows::UI::Xaml::Controls::Control GetControl();
        Windows::UI::Xaml::UIElement GetRoot();

        void Close();
        hstring GetTitle();
        Microsoft::Terminal::TerminalControl::TermControl Terminal();
        Windows::Foundation::Size MinimumSize() const;

        bool IsFocused();
        void Focus();

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);

    private:
        Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
    };
}
namespace winrt::TerminalApp::factory_implementation
{
    struct TermControlHost : TermControlHostT<TermControlHost, implementation::TermControlHost>
    {
    };
}
