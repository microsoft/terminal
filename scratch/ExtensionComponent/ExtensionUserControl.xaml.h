//
// ExtensionUserControl.xaml.h
// Declaration of the ExtensionUserControl class
//

#pragma once

#include "ExtensionUserControl.g.h"

namespace winrt::ExtensionComponent::implementation
{
    struct ExtensionUserControl : ExtensionUserControlT<ExtensionUserControl>
    {
        ExtensionUserControl() = default;

    public:
        // and an observable "MyValue" property
        int32_t MyValue();
        void MyValue(const int32_t& value);

        winrt::event_token PropertyChanged(winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return _PropertyChanged.add(handler);
        }
        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            _PropertyChanged.remove(token);
        }

    private:
        int32_t _MyValue{ 10 };
        winrt::event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> _PropertyChanged;
    };
}

namespace winrt::ExtensionComponent::factory_implementation
{
    struct ExtensionUserControl : ExtensionUserControlT<ExtensionUserControl, implementation::ExtensionUserControl>
    {
    };
}
