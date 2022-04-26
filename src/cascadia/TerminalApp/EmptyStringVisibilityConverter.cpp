#include "pch.h"
#include "EmptyStringVisibilityConverter.h"
#include "EmptyStringVisibilityConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Attempt to convert something into another type. For the
    //   EmptyStringVisibilityConverter, we're gonna check if `value` is a
    //   string, and try and convert it into a Visibility value. If the input
    //   param wasn't a string, or was the empty string, we'll return
    //   Visibility::Collapsed. Otherwise, we'll return Visible.

    // Arguments:
    // - value: the input object to attempt to convert into a Visibility.
    // Return Value:
    // - Visible if the object was a string and wasn't the empty string.
    Foundation::IInspectable EmptyStringVisibilityConverter::Convert(const Foundation::IInspectable& value,
                                                                     const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                                     const Foundation::IInspectable& /* parameter */,
                                                                     const hstring& /* language */)
    {
        const auto& name = winrt::unbox_value_or<hstring>(value, L"");
        return winrt::box_value(name.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    // unused for one-way bindings
    Foundation::IInspectable EmptyStringVisibilityConverter::ConvertBack(const Foundation::IInspectable& /* value */,
                                                                         const Windows::UI::Xaml::Interop::TypeName& /* targetType */,
                                                                         const Foundation::IInspectable& /* parameter */,
                                                                         const hstring& /* language */)
    {
        throw hresult_not_implemented();
    }
}
