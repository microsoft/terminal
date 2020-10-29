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

        std::wstring temp{ text.c_str() };
        std::transform(temp.begin(), temp.end(), temp.begin(), [](auto c) { return static_cast<char>(std::tolower(c)); });
        if (temp == L"extrablack")
        {
            weight.Weight = 950;
        }
        else if (temp == L"black")
        {
            weight.Weight = 900;
        }
        else if (temp == L"extrabold")
        {
            weight.Weight = 800;
        }
        else if (temp == L"bold")
        {
            weight.Weight = 700;
        }
        else if (temp == L"semibold")
        {
            weight.Weight = 600;
        }
        else if (temp == L"medium")
        {
            weight.Weight = 500;
        }
        else if (temp == L"normal")
        {
            weight.Weight = 400;
        }
        else if (temp == L"semilight")
        {
            weight.Weight = 350;
        }
        else if (temp == L"light")
        {
            weight.Weight = 300;
        }
        else if (temp == L"extralight")
        {
            weight.Weight = 200;
        }
        else if (temp == L"thin")
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
