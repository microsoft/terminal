// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PaddingConverter.h"
#include "PaddingConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Foundation::IInspectable PaddingConverter::Convert(Foundation::IInspectable const& value,
                                                       Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                       Foundation::IInspectable const& /* parameter */,
                                                       hstring const& /* language */)
    {
        const auto& padding = winrt::unbox_value<hstring>(value);

        const wchar_t singleCharDelim = L',';
        std::wstringstream tokenStream(padding.c_str());
        std::wstring token;
        double maxVal = 0;
        size_t* idx = nullptr;

        // Get padding values till we run out of delimiter separated values in the stream
        // Non-numeral values detected will default to 0
        // std::getline will not throw exception unless flags are set on the wstringstream
        // std::stod will throw invalid_argument exception if the input is an invalid double value
        // std::stod will throw out_of_range exception if the input value is more than DBL_MAX
        try
        {
            while (std::getline(tokenStream, token, singleCharDelim))
            {
                // std::stod internally calls wcstod which handles whitespace prefix (which is ignored)
                //  & stops the scan when first char outside the range of radix is encountered
                // We'll be permissive till the extent that stod function allows us to be by default
                // Ex. a value like 100.3#535w2 will be read as 100.3, but ;df25 will fail
                const auto curVal = std::stod(token, idx);
                if (curVal > maxVal)
                {
                    maxVal = curVal;
                }
            }
        }
        catch (...)
        {
            // If something goes wrong, even if due to a single bad padding value, we'll return default 0 padding
            maxVal = 0;
            LOG_CAUGHT_EXCEPTION();
        }

        return winrt::box_value<double>(maxVal);
    }

    Foundation::IInspectable PaddingConverter::ConvertBack(Foundation::IInspectable const& value,
                                                           Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                           Foundation::IInspectable const& /*parameter*/,
                                                           hstring const& /* language */)
    {
        const auto padding{ winrt::unbox_value<double>(value) };
        return winrt::box_value(winrt::to_hstring(padding));
    }
}
