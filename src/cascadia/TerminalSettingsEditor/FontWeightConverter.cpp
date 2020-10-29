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
        std::string text = unbox_value<std::string>(value);
        try
        {
            std::transform(text.begin(), text.end(), text.begin(), [](auto c) { return static_cast<char>(std::tolower(c)); });
            if (text == "extrablack")
            {
                weight.Weight = 950;
            } 
            else if (text == "black")
            {
                weight.Weight = 900;
            }
            else if (text == "extrabold")
            {
                weight.Weight = 800;
            }
            else if (text == "bold")
            {
                weight.Weight = 700;
            }
            else if (text == "semibold")
            {
                weight.Weight = 600;
            }
            else if (text == "medium")
            {
                weight.Weight = 500;
            }
            else if (text == "normal")
            {
                weight.Weight = 400;
            }
            else if (text == "semilight")
            {
                weight.Weight = 350;
            }
            else if (text == "light")
            {
                weight.Weight = 300;
            }
            else if (text == "extralight")
            {
                weight.Weight = 200;
            }
            else if (text == "thin")
            {
                weight.Weight = 100;
            }           
        }
        catch (...)
        {
            std::stoi(text);
            weight.Weight = static_cast<uint16_t>(winrt::unbox_value<UINT16>(value));
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
