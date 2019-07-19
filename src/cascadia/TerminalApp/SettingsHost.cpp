#include "pch.h"
#include "SettingsHost.h"
#include "SettingsHost.g.cpp"
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Media;

namespace winrt::TerminalApp::implementation
{
    SettingsHost::SettingsHost()
    {
        InitializeComponent();
    }

    Windows::UI::Xaml::UIElement SettingsHost::GetRoot()
    {
        return *this;
    }

    void SettingsHost::Close()
    {
        throw hresult_not_implemented();
    }

    hstring SettingsHost::GetTitle()
    {
        return L"Settings";
    }

    winrt::Windows::Foundation::Size SettingsHost::MinimumSize() const
    {
        return { 32, 32 };
    }

    bool SettingsHost::IsFocused()
    {
        return _Editor().FocusState() != FocusState::Unfocused;
    }
    void SettingsHost::Focus()
    {
        _Editor().Focus(FocusState::Programmatic);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SettingsHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SettingsHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
