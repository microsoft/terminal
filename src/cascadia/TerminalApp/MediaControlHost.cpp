#include "pch.h"
#include "MediaControlHost.h"
#include "MediaControlHost.g.cpp"
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Media;

namespace winrt::TerminalApp::implementation
{
    fire_and_forget GetSystemMedia()
    {
        co_await winrt::resume_background();
        auto thing = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        auto mgr = thing.get();
        // auto foo = co_await thing.get();
        auto session = mgr.GetCurrentSession();
        auto foo = session.GetPlaybackInfo();
        auto mediaAsync = session.TryGetMediaPropertiesAsync();
        auto media = mediaAsync.get();
        auto artist = media.AlbumArtist();
        auto title = media.Title();
        std::wstring realTitle = title.c_str();
        std::wstring realArtist = artist.c_str();
    }

    MediaControlHost::MediaControlHost()
    {
        InitializeComponent();
        // auto session = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::GetCurrentSession();
        // session.z

        //     winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager mgr{};
        // // winrt::Windows::Media::GlobalSystemMediaTransportControlsSessionManager mgr{};

        // winrt::Windows::Media::SystemMediaTransportControls _control{ nullptr };
        // _control = SystemMediaTransportControls::GetForCurrentView();
        // // _control.IsPlayEnabled(true);
        // if (_control)
        // {
        //     auto du = _control.DisplayUpdater();
        //     auto musicProps = du.MusicProperties();
        //     auto videoProps = du.VideoProperties();
        //     if (musicProps)
        //     {
        //         auto songTitle = du.MusicProperties().Title();
        //         this->_Title().Text(songTitle);
        //     }
        // }
        // // = SystemMediaTransportControls.GetForCurrentView();
        Loaded([this](auto&&, auto&&) {
            GetSystemMedia();
        });
    }

    // Windows::UI::Xaml::Controls::Control MediaControlHost::GetControl()
    // {
    //     return _Editor();
    // }

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
        return _Editor().FocusState() != FocusState::Unfocused;
    }
    void MediaControlHost::Focus()
    {
        _Editor().Focus(FocusState::Programmatic);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MediaControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
