#include "pch.h"
#include "MediaControlHost.h"
#include "MediaControlHost.g.cpp"
using namespace winrt::Windows::Foundation;
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

        auto info = session.GetPlaybackInfo();
        _playbackState = info.PlaybackStatus();

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

        _PreviousButton().Click({ this, &MediaControlHost::_PreviousClick });
        _PlayPauseButton().Click({ this, &MediaControlHost::_PlayPauseClick });
        _NextButton().Click({ this, &MediaControlHost::_NextClick });

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

    void MediaControlHost::_PreviousClick(IInspectable const& sender,
                                          RoutedEventArgs const& e)
    {
        _DispatchPreviousClick();
    }
    void MediaControlHost::_NextClick(IInspectable const& sender,
                                      RoutedEventArgs const& e)
    {
        _DispatchNextClick();
    }
    void MediaControlHost::_PlayPauseClick(IInspectable const& sender,
                                           RoutedEventArgs const& e)
    {
        _DispatchPlayPauseClick();
    }

    fire_and_forget MediaControlHost::_DispatchPreviousClick()
    {
        co_await winrt::resume_background();
        if (_session)
        {
            if (_playbackState == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
            {
                _session.TrySkipPreviousAsync();
            }
        }
    }

    fire_and_forget MediaControlHost::_DispatchNextClick()
    {
        co_await winrt::resume_background();
        if (_session)
        {
            if (_playbackState == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
            {
                _session.TrySkipNextAsync();
            }
        }
    }

    fire_and_forget MediaControlHost::_DispatchPlayPauseClick()
    {
        co_await winrt::resume_background();
        if (_session)
        {
            auto foo = _playbackState;
            if (_playbackState == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing)
            {
                _session.TryPauseAsync();
            }
            else if (_playbackState == Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused)
            {
                _session.TryPlayAsync();
            }
        }
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
