#include "pch.h"
#include "Converters.h"
#if __has_include("Converters.g.cpp")
#include "Converters.g.cpp"
#endif

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    winrt::hstring Converters::AppendPercentageSign(double value)
    {
        const auto number{ value };
        return to_hstring((int)number) + L"%";
    }

    WUXMedia::SolidColorBrush Converters::ColorToBrush(winrt::Windows::UI::Color color)
    {
        return WUXMedia::SolidColorBrush(color);
    }

    WUT::FontWeight Converters::DoubleToFontWeight(double value)
    {
        return WUT::FontWeight{ base::ClampedNumeric<uint16_t>(value) };
    }

    double Converters::FontWeightToDouble(WUT::FontWeight fontWeight)
    {
        return fontWeight.Weight;
    }

    bool Converters::InvertBoolean(bool value)
    {
        return !value;
    }

    WUX::Visibility Converters::InvertedBooleanToVisibility(bool value)
    {
        return value ? WUX::Visibility::Collapsed : WUX::Visibility::Visible;
    }

    double Converters::MaxValueFromPaddingString(winrt::hstring paddingString)
    {
        const auto singleCharDelim = L',';
        std::wstringstream tokenStream(paddingString.c_str());
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

        return maxVal;
    }

    int Converters::PercentageToPercentageValue(double value)
    {
        return base::ClampMul(value, 100u);
    }

    double Converters::PercentageValueToPercentage(double value)
    {
        return base::ClampDiv<double, double>(value, 100);
    }

    bool Converters::StringsAreNotEqual(winrt::hstring expected, winrt::hstring actual)
    {
        return expected != actual;
    }
    WUX::Visibility Converters::StringNotEmptyToVisibility(winrt::hstring value)
    {
        return value.empty() ? WUX::Visibility::Collapsed : WUX::Visibility::Visible;
    }

    // Method Description:
    // - Returns the value string, unless it matches the placeholder in which case the empty string.
    // Arguments:
    // - placeholder - the placeholder string.
    // - value - the value string.
    // Return Value:
    // - The value string, unless it matches the placeholder in which case the empty string.
    winrt::hstring Converters::StringOrEmptyIfPlaceholder(winrt::hstring placeholder, winrt::hstring value)
    {
        return placeholder == value ? L"" : value;
    }
}
