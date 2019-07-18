#include "pch.h"
#include "RichTextBoxControlHost.h"
#include "RichTextBoxControlHost.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::TerminalApp::implementation
{
    RichTextBoxControlHost::RichTextBoxControlHost()
    {
        InitializeComponent();

    }

    Windows::UI::Xaml::Controls::Control RichTextBoxControlHost::GetControl()
    {
        return _Editor();
    }

    Windows::UI::Xaml::UIElement RichTextBoxControlHost::GetRoot()
    {
        return *this;
    }

    void RichTextBoxControlHost::Close()
    {
        throw hresult_not_implemented();
    }

    hstring RichTextBoxControlHost::GetTitle()
    {
        return L"foo";
    }

    winrt::Windows::Foundation::Size RichTextBoxControlHost::MinimumSize() const
    {
        return { 32, 32 };
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(RichTextBoxControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(RichTextBoxControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
