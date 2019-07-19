#include "pch.h"
#include "WebViewHost.h"
#include "WebViewHost.g.cpp"
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Media;

namespace winrt::TerminalApp::implementation
{
    WebViewHost::WebViewHost()
    {
        InitializeComponent();
        _webView = Controls::WebView{};

        Children().Append(_webView);

        // Controls::Grid::SetRow(_webView, 2);

        _webView.Loaded([this](auto&&, auto&&) {
            auto w = _webView.ActualWidth();
            auto h = _webView.ActualHeight();
            try
            {
                _webView.Navigate(winrt::Windows::Foundation::Uri(L"https://www.github.com/microsoft/terminal"));
                // _webView.NavigateToString(L"<html><body><h2>This is an HTML fragment</h2></body></html>");
            }
            CATCH_LOG();
            /*catch (...)
            {
                auto b = 0;
                b++;
                b;
            }*/
        });
        _webView.NavigationStarting([this](auto&&, auto&&) {
            auto a = 0;
            a++;
            a;
        });
    }

    Windows::UI::Xaml::Controls::Control WebViewHost::GetControl()
    {
        return _Editor();
    }

    Windows::UI::Xaml::UIElement WebViewHost::GetRoot()
    {
        return *this;
    }

    void WebViewHost::Close()
    {
        throw hresult_not_implemented();
    }

    hstring WebViewHost::GetTitle()
    {
        return L"foo";
    }

    winrt::Windows::Foundation::Size WebViewHost::MinimumSize() const
    {
        return { 32, 32 };
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(WebViewHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(WebViewHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
