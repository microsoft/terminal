#include "pch.h"
#include "TextBlockControlHost.h"
#include "TextBlockControlHost.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::TerminalApp::implementation
{
    TextBlockControlHost::TextBlockControlHost() :
        _textBox{}
    {
        _textBox.HorizontalAlignment(HorizontalAlignment::Stretch);
        _textBox.VerticalAlignment(VerticalAlignment::Stretch);

        // _textBox.IsEnabled(true);
        // _textBox.Text(L"I am a TextBox");
    }

    Windows::UI::Xaml::Controls::Control TextBlockControlHost::GetControl()
    {
        return _textBox;
    }
    Windows::UI::Xaml::UIElement TextBlockControlHost::GetRoot()
    {
        return _textBox;
    }

    void TextBlockControlHost::Close()
    {
        throw hresult_not_implemented();
    }

    hstring TextBlockControlHost::GetTitle()
    {
        return L"foo";
    }

    winrt::Windows::Foundation::Size TextBlockControlHost::MinimumSize() const
    {
        return { 32, 32 };
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TextBlockControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TextBlockControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
