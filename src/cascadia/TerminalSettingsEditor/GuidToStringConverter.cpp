#include "pch.h"
#include "GuidToStringConverter.h"
#include "GuidToStringConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable GuidToStringConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        winrt::guid test{ winrt::unbox_value<winrt::guid>(value) };
        return winrt::box_value(to_hstring(test));
    }

    Foundation::IInspectable GuidToStringConverter::ConvertBack(Foundation::IInspectable const& value,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /* parameter */,
                                                              hstring const& /* language */)
    {
        auto str{ winrt::unbox_value<winrt::hstring>(value) };
        GUID result{};
        IIDFromString(str.c_str(), &result);
        return winrt::box_value<winrt::guid>(result);
    }
}
