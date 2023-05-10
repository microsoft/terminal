#pragma once

#include "ExtensionClass.g.h"

namespace winrt::ExtensionComponent::implementation
{
    struct ExtensionClass : ExtensionClassT<ExtensionClass>
    {
        ExtensionClass() = default;

        int32_t MyProperty();
        void MyProperty(int32_t value);
        int32_t DoTheThing();
        winrt::Windows::UI::Xaml::FrameworkElement PaneContent();

        // typed event handler for the SendInputRequested event
        winrt::event_token SendInputRequested(winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::SampleExtensions::SendInputArgs> const& handler)
        {
            return _SendInputRequestedHandlers.add(handler);
        }
        void SendInputRequested(winrt::event_token const& token) noexcept
        {
            _SendInputRequestedHandlers.remove(token);
        }

    private:
        winrt::fire_and_forget _makeWebView(const winrt::Windows::UI::Xaml::Controls::StackPanel parent);
        void _webMessageReceived(const winrt::Windows::Foundation::IInspectable& sender,
                                 winrt::Microsoft::Web::WebView2::Core::CoreWebView2WebMessageReceivedEventArgs args);

        winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Foundation::IInspectable, winrt::SampleExtensions::SendInputArgs>> _SendInputRequestedHandlers;
    };
}

namespace winrt::ExtensionComponent::factory_implementation
{
    struct ExtensionClass : ExtensionClassT<ExtensionClass, implementation::ExtensionClass>
    {
    };
}
