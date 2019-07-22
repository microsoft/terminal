#pragma once
#include "MediaControlHost.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Media.Control.h>

namespace winrt::TerminalApp::implementation
{
    struct MediaControlHost : MediaControlHostT<MediaControlHost>
    {
        MediaControlHost();

        // Windows::UI::Xaml::Controls::Control GetControl();
        Windows::UI::Xaml::UIElement GetRoot();
        void Close();
        hstring GetTitle();
        Windows::Foundation::Size MinimumSize() const;

        bool IsFocused();
        void Focus();

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);

    private:
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession _session{ nullptr };
        //     winrt::Windows::UI::Xaml::Controls::RichEditBox _textBox{ nullptr };
        fire_and_forget _SetupMediaManager();
        void _MediaPropertiesChanged(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session,
                                     winrt::Windows::Media::Control::MediaPropertiesChangedEventArgs args);
        void _PlaybackInfoChanged(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session,
                                  winrt::Windows::Media::Control::PlaybackInfoChangedEventArgs args);
        // void _TimelinePropertiesChanged(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session,
        //                              winrt::Windows::Media::Control::MediaPropertiesChangedEventArgs args);
        void _UpdateMediaInfo(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session);
    };
}
namespace winrt::TerminalApp::factory_implementation
{
    struct MediaControlHost : MediaControlHostT<MediaControlHost, implementation::MediaControlHost>
    {
    };
}
