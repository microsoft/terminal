#pragma once
#include "MediaControlHost.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <winrt/Windows.Media.h>

namespace winrt::TerminalApp::implementation
{
    struct MediaControlHost : MediaControlHostT<MediaControlHost>
    {
        MediaControlHost();

        Windows::UI::Xaml::Controls::Control GetControl();
        Windows::UI::Xaml::UIElement GetRoot();
        void Close();
        hstring GetTitle();
        Windows::Foundation::Size MinimumSize() const;

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);

        // private:
        //     winrt::Windows::UI::Xaml::Controls::RichEditBox _textBox{ nullptr };
    };
}
namespace winrt::TerminalApp::factory_implementation
{
    struct MediaControlHost : MediaControlHostT<MediaControlHost, implementation::MediaControlHost>
    {
    };
}
