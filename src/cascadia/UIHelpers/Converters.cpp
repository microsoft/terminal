#include "pch.h"
#include "Converters.h"
#include "Converters.g.cpp"

#pragma warning(disable : 26497) // We will make these functions constexpr, as they are part of an ABI boundary.
#pragma warning(disable : 26440) // The function ... can be declared as noexcept.

namespace winrt::Microsoft::Terminal::UI::implementation
{
    // Booleans
    bool Converters::InvertBoolean(bool value)
    {
        return !value;
    }

    winrt::Windows::UI::Xaml::Visibility Converters::InvertedBooleanToVisibility(bool value)
    {
        return value ? winrt::Windows::UI::Xaml::Visibility::Collapsed : winrt::Windows::UI::Xaml::Visibility::Visible;
    }

    // Numbers
    double Converters::PercentageToPercentageValue(double value)
    {
        return value * 100.0;
    }

    double Converters::PercentageValueToPercentage(double value)
    {
        return value / 100.0;
    }

    winrt::hstring Converters::PercentageToPercentageString(double value)
    {
        return winrt::hstring{ fmt::format(FMT_COMPILE(L"{:.0f}%"), value * 100.0) };
    }

    // Strings
    bool Converters::StringsAreNotEqual(const winrt::hstring& expected, const winrt::hstring& actual)
    {
        return expected != actual;
    }

    bool Converters::StringNotEmpty(const winrt::hstring& value)
    {
        return !value.empty();
    }

    winrt::Windows::UI::Xaml::Visibility Converters::StringNotEmptyToVisibility(const winrt::hstring& value)
    {
        return value.empty() ? winrt::Windows::UI::Xaml::Visibility::Collapsed : winrt::Windows::UI::Xaml::Visibility::Visible;
    }

    winrt::hstring Converters::StringOrEmptyIfPlaceholder(const winrt::hstring& placeholder, const winrt::hstring& value)
    {
        return placeholder == value ? winrt::hstring{} : value;
    }

    // Misc
    winrt::Windows::UI::Text::FontWeight Converters::DoubleToFontWeight(double value)
    {
        uint16_t val = 400;

        if (value >= 1.0 && value <= 1000.0)
        {
            val = gsl::narrow_cast<uint16_t>(lrint(value));
        }

        return winrt::Windows::UI::Text::FontWeight{ val };
    }

    winrt::Windows::UI::Xaml::Media::SolidColorBrush Converters::ColorToBrush(const winrt::Windows::UI::Color color)
    {
        return Windows::UI::Xaml::Media::SolidColorBrush(color);
    }

    double Converters::FontWeightToDouble(const winrt::Windows::UI::Text::FontWeight fontWeight)
    {
        return fontWeight.Weight;
    }

    double Converters::MaxValueFromPaddingString(const winrt::hstring& paddingString)
    {
        std::wstring buffer;
        double maxVal = 0;

        auto& errnoRef = errno; // Nonzero cost, pay it once

        // Get padding values till we run out of delimiter separated values in the stream
        // Non-numeral values detected will default to 0
        // std::stod will throw invalid_argument exception if the input is an invalid double value
        // std::stod will throw out_of_range exception if the input value is more than DBL_MAX
        for (const auto& part : til::split_iterator{ std::wstring_view{ paddingString }, L',' })
        {
            buffer.assign(part);

            // wcstod handles whitespace prefix (which is ignored) & stops the
            // scan when first char outside the range of radix is encountered.
            // We'll be permissive till the extent that stod function allows us to be by default
            // Ex. a value like 100.3#535w2 will be read as 100.3, but ;df25 will fail
            errnoRef = 0;
            wchar_t* end;
            const double val = wcstod(buffer.c_str(), &end);

            if (end != buffer.c_str() && errnoRef != ERANGE)
            {
                maxVal = std::max(maxVal, val);
            }
        }

        return maxVal;
    }
}
