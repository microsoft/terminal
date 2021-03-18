// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChordConverter.h"
#include "KeyChordToStringConverter.g.cpp"
#include "KeyChordToVisibilityConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable KeyChordToStringConverter::Convert(Foundation::IInspectable const& value,
                                                                Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                Foundation::IInspectable const& /* parameter */,
                                                                hstring const& /* language */)
    {
        const auto& keys{ winrt::unbox_value<Control::KeyChord>(value) };
        return box_value(Model::KeyChordSerialization::ToString(keys));
    }

    Foundation::IInspectable KeyChordToStringConverter::ConvertBack(Foundation::IInspectable const& value,
                                                                    Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                    Foundation::IInspectable const& /*parameter*/,
                                                                    hstring const& /* language */)
    {
        const auto& keys{ winrt::unbox_value<hstring>(value) };
        return box_value(Model::KeyChordSerialization::FromString(keys));
    }

    Foundation::IInspectable KeyChordToVisibilityConverter::Convert(Foundation::IInspectable const& value,
                                                                    Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                    Foundation::IInspectable const& /* parameter */,
                                                                    hstring const& /* language */)
    {
        return box_value(value ? Visibility::Visible : Visibility::Collapsed);
    }

    Foundation::IInspectable KeyChordToVisibilityConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                                        Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                        Foundation::IInspectable const& /*parameter*/,
                                                                        hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
