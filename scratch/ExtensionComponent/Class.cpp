#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

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

    winrt::Windows::UI::Xaml::FrameworkElement Class::PaneContent()
    {
        winrt::Windows::UI::Xaml::Controls::Button myButton{};
        myButton.Content(winrt::box_value(L"This came from an extension"));

        // Add an onclick handler to the button that sets the background of the button to a random color
        myButton.Click([myButton](auto&&, auto&&) {
            winrt::Windows::UI::Xaml::Media::SolidColorBrush brush{};
            brush.Color(winrt::Windows::UI::ColorHelper::FromArgb(255, rand() % 255, rand() % 255, rand() % 255));
            myButton.Background(brush);
        });
        return myButton;
    }

}
