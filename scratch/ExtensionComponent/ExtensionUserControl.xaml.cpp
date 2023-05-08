//
// ExtensionUserControl.xaml.cpp
// Implementation of the ExtensionUserControl class
//

#include "pch.h"
#include "ExtensionUserControl.xaml.h"
#include "ExtensionUserControl.g.cpp"



// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace winrt::ExtensionComponent::implementation
{

    int32_t ExtensionUserControl::MyValue()
    {
        return _MyValue;
    };
    void ExtensionUserControl::MyValue(const int32_t& value)
    {
        if (_MyValue != value)
        {
            _MyValue = value;
            _PropertyChanged(*this, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"MyValue" });
        }
    };
}
