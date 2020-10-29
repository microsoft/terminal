// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FontWeightConverter.h"
#include "FontWeightConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable FontWeightConverter::Convert(Foundation::IInspectable const& value,
                                                          Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                          Foundation::IInspectable const& /* parameter */,
                                                          hstring const& /* language */)
    {
        Windows::UI::Text::FontWeight weight;
        hstring text = unbox_value<hstring>(value);

        text.c_str();
        std::transform(text.begin(), text.end(), text.begin(), [](auto c) { return static_cast<char>(std::tolower(c)); });
        if (text == L"extrablack")
        {
            weight.Weight = 950;
        }
        else if (text == L"black")
        {
            weight.Weight = 900;
        }
        else if (text == L"extrabold")
        {
            weight.Weight = 800;
        }
        else if (text == L"bold")
        {
            weight.Weight = 700;
        }
        else if (text == L"semibold")
        {
            weight.Weight = 600;
        }
        else if (text == L"medium")
        {
            weight.Weight = 500;
        }
        else if (text == L"normal")
        {
            weight.Weight = 400;
        }
        else if (text == L"semilight")
        {
            weight.Weight = 350;
        }
        else if (text == L"light")
        {
            weight.Weight = 300;
        }
        else if (text == L"extralight")
        {
            weight.Weight = 200;
        }
        else if (text == L"thin")
        {
            weight.Weight = 100;
        }

        try
        {
            weight.Weight = static_cast<uint16_t>((std::stoi(text.c_str())));             
        }
        catch (...)
        {
            weight.Weight = 400;
        }
        
        return winrt::box_value(weight);
    }

    Foundation::IInspectable FontWeightConverter::ConvertBack(Foundation::IInspectable const& /*value*/,
                                                              Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                              Foundation::IInspectable const& /* parameter */,
                                                              hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
