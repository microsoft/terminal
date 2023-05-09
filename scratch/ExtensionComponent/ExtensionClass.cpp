#include "pch.h"
#include "ExtensionClass.h"
#include "ExtensionClass.g.cpp"

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::ExtensionComponent::implementation
{
    int32_t ExtensionClass::MyProperty()
    {
        return 99;
    }

    void ExtensionClass::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    int32_t ExtensionClass::DoTheThing()
    {
        return 101;
    }

    winrt::fire_and_forget ExtensionClass::_webMessageReceived(const IInspectable& sender,
                                                      const winrt::Microsoft::Web::WebView2::Core::CoreWebView2WebMessageReceivedEventArgs& args)
    {
        auto message{ args.TryGetWebMessageAsString() };
        if (!message.empty())
        {
            auto uri = winrt::Windows::Foundation::Uri(message);
            if (uri.SchemeName() == L"sendinput")
            {
                //MyButtonHandler(uri);
                auto query = winrt::Windows::Foundation::WwwFormUrlDecoder(uri.Query());
                query;
            }
        }

        co_return;
    }

    winrt::fire_and_forget ExtensionClass::_makeWebView(const WUX::Controls::StackPanel parent)
    {
        winrt::MUX::Controls::WebView2 wv{ nullptr };
        wv = winrt::MUX::Controls::WebView2();
        wv.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        wv.Height(300);
        parent.Children().Append(wv);
        co_await wv.EnsureCoreWebView2Async();

        std::string page = R"_(
<html>


<body>
<h1>My First Heading</h1>
Hello world

<form id="myForm">
  <label for="myInput">Enter text:</label>
  <input type="text" id="myInput" name="myInput">
  <button type="submit" id="myButton">Submit</button>
</form>

</body>


<script>
document.getElementById("myForm").addEventListener("submit", function(event) {
  event.preventDefault();
  var input = document.getElementById("myInput").value;
  window.chrome.webview.postMessage("sendInput://?text=" + encodeURIComponent(input));
});
</script>

</html>
    )_";

        wv.WebMessageReceived({ this, &ExtensionClass::_webMessageReceived });

        // wv.CoreWebView2().Navigate(L"https://www.github.com/microsoft/terminal");
        wv.CoreWebView2().NavigateToString(winrt::to_hstring(page));
    }

    winrt::Windows::UI::Xaml::FrameworkElement ExtensionClass::PaneContent()
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
