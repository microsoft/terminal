// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorToHexConverter.h"
#include "ColorToHexConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable ColorToHexConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        til::color color{ winrt::unbox_value<winrt::Windows::UI::Color>(value) };
        auto hex = winrt::to_hstring(color.ToHexString().data());
        return winrt::box_value(hex);
    }

    Foundation::IInspectable ColorToHexConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /*parameter*/,
                                                              hstring const& /* language */)
    {
        //auto str = winrt::unbox_value<winrt::hstring>(value);
        //try
        //{
        //    auto color = til::color::HexToColor(str.data());
        //    Windows::UI::Color newColor = color;
        //    return winrt::box_value(newColor);
        //}
        //catch (...)
        //{
        //    // TODO: Not sure how to return the text before this invalid text.
        //    //       For now, I'll just return black as a default color if the string is invalid.
        //    //
        //    // The alternative is to get rid of ConvertBack, and implement
        //    // handlers for GotFocus/LostFocus. GotFocus would save the current hex
        //    // string in the TextBox, and LostFocus would then perform validation
        //    // on the TextBox.Text, and if validation fails, revert to the string saved
        //    // in GotFocus.
        //    // However, this approach would require me to update the colors manually
        //    // in codebehind in the LostFocus handler, which also means I need some way
        //    // of identifying what color the handler was fired off, probably through the
        //    // Tag property.
        //    return winrt::box_value(Windows::UI::Color{ 0, 0, 0, 0 });
        //}'
        throw hresult_not_implemented();
    }
}
