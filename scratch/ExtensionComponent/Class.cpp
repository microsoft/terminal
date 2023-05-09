#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::ExtensionComponent::implementation
{
    int32_t Class::MyProperty()
    {
        return 99;
    }

    void Class::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    int32_t Class::DoTheThing()
    {
        return 101;
    }

    winrt::fire_and_forget _makeWebView(const WUX::Controls::StackPanel parent)
    {
        winrt::MUX::Controls::WebView2 wv{ nullptr };
        wv = winrt::MUX::Controls::WebView2();
        wv.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        wv.Height(300);
        parent.Children().Append(wv);
        co_await wv.EnsureCoreWebView2Async();
        wv.CoreWebView2().Navigate(L"https://www.github.com/microsoft/terminal");
    }

    winrt::Windows::UI::Xaml::FrameworkElement Class::PaneContent()
    {
        // winrt::Windows::UI::Xaml::Controls::Button myButton{};
        // myButton.Content(winrt::box_value(L"This came from an extension"));
        // // Add an onclick handler to the button that sets the background of the button to a random color
        // myButton.Click([myButton](auto&&, auto&&) {
        //     winrt::Windows::UI::Xaml::Media::SolidColorBrush brush{};
        //     brush.Color(winrt::Windows::UI::ColorHelper::FromArgb(255, rand() % 255, rand() % 255, rand() % 255));
        //     myButton.Background(brush);
        // });
        // return myButton;

        // // This may be hard to impossible, due to
        // // https://github.com/microsoft/microsoft-ui-xaml/issues/6299
        // winrt::ExtensionComponent::ExtensionUserControl myControl{};
        // return myControl;

        winrt::WUX::Controls::StackPanel sp{};
        sp.Orientation(WUX::Controls::Orientation::Vertical);
        sp.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        sp.VerticalAlignment(WUX::VerticalAlignment::Stretch);
        _makeWebView(sp);
        return sp;
    }

}
