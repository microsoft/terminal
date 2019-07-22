#include "pch.h"
#include "MediaControlHost.h"
#include "MediaControlHost.g.cpp"
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Media;

namespace winrt::TerminalApp::implementation
{
    void MediaControlHost::_UpdateMediaInfo(Control::GlobalSystemMediaTransportControlsSession session)
    {
        auto mediaAsync = session.TryGetMediaPropertiesAsync();
        auto media = mediaAsync.get();
        auto artist = media.AlbumArtist();
        auto title = media.Title();
        std::wstring realTitle = title.c_str();
        std::wstring realArtist = artist.c_str();

        auto info = session.GetPlaybackInfo();

        Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this, artist, title, info]() {
            _Title().Text(title);
            _Band().Text(artist);

            auto status = info.PlaybackStatus();
            if (status == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
            {
                _PlayPauseIcon().Glyph(L"\xE769");
            }
            else if (status == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused)
            {
                _PlayPauseIcon().Glyph(L"\xE768");
            }
        });
    }

    void MediaControlHost::_MediaPropertiesChanged(Control::GlobalSystemMediaTransportControlsSession session,
                                                   Control::MediaPropertiesChangedEventArgs args)
    {
        _UpdateMediaInfo(session);
    }

    void MediaControlHost::_PlaybackInfoChanged(Control::GlobalSystemMediaTransportControlsSession session,
                                                Control::PlaybackInfoChangedEventArgs args)
    {
        _UpdateMediaInfo(session);

        auto mediaAsync = session.TryGetMediaPropertiesAsync();
        auto media = mediaAsync.get();
        auto info = session.GetPlaybackInfo();
        auto status = info.PlaybackStatus();

        if (status == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
        {
            _PlayPauseIcon().Glyph(L"&#xE769");
        }
        else if (status == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused)
        {
            _PlayPauseIcon().Glyph(L"&#xE768");
        }
    }

    fire_and_forget MediaControlHost::_SetupMediaManager()
    {
        co_await winrt::resume_background();
        auto request = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        auto mgr = request.get();

        _session = mgr.GetCurrentSession();
        _session.MediaPropertiesChanged({ this, &MediaControlHost::_MediaPropertiesChanged });
        mgr.CurrentSessionChanged([this](auto&&, auto&&) {
            auto a = 0;
            a++;
            a;
        });
        _session.PlaybackInfoChanged({ this, &MediaControlHost::_PlaybackInfoChanged });
        // _session.TimelinePropertiesChanged([this](auto&&, auto&&) {
        //     auto a = 0;
        //     a++;
        //     a;
        // });
        _UpdateMediaInfo(_session);
    }

    MediaControlHost::MediaControlHost()
    {
        InitializeComponent();

        Loaded([this](auto&&, auto&&) {
            _SetupMediaManager();
        });
    }

    Windows::UI::Xaml::UIElement MediaControlHost::GetRoot()
    {
        return *this;
    }

    void MediaControlHost::Close()
    {
        throw hresult_not_implemented();
    }

    hstring MediaControlHost::GetTitle()
    {
        return L"foo";
    }

    winrt::Windows::Foundation::Size MediaControlHost::MinimumSize() const
    {
        return { 32, 32 };
    }

    bool MediaControlHost::IsFocused()
    {
        // TODO: Return if focus is anywhere in our tree
        return false;
        // return _Editor().FocusState() != FocusState::Unfocused;
    }
    void MediaControlHost::Focus()
    {
        _PlayPauseButton().Focus(FocusState::Programmatic);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
