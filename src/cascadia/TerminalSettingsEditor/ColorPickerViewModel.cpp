// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorPickerViewModel.h"

#include "ColorRef.g.cpp"
#include "ColorPickerViewModel.g.cpp"
#include "TerminalColorToBrushConverter.g.cpp"
#include "TerminalColorToStringConverter.g.cpp"

using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{

    Windows::Foundation::IInspectable TerminalColorToBrushConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        if (const auto termColor = value.as<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>())
        {
            Windows::UI::Color color{
                .A = 255,
                .R = termColor.Value().R,
                .G = termColor.Value().G,
                .B = termColor.Value().B
            };
            return Microsoft::Terminal::UI::Converters::ColorToBrush(color);
        }
        return nullptr;
    }

    Windows::Foundation::IInspectable TerminalColorToBrushConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }

    Windows::Foundation::IInspectable TerminalColorToStringConverter::Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        if (const auto maybeColor = value.as<Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>>())
        {
            const auto color = maybeColor.Value();
            return winrt::box_value(fmt::format(FMT_COMPILE(L"#{:02X}{:02X}{:02X}"), color.R, color.G, color.B));
        }
        return nullptr;
    }

    Windows::Foundation::IInspectable TerminalColorToStringConverter::ConvertBack(Windows::Foundation::IInspectable const& /*value*/, Windows::UI::Xaml::Interop::TypeName const& /*targetType*/, Windows::Foundation::IInspectable const& /*parameter*/, hstring const& /*language*/)
    {
        throw hresult_not_implemented();
    }

    ColorRef::ColorRef(const Windows::UI::Color& color) :
        _Color{ color },
        _Type{ ColorType::RGB }
    {
    }

    ColorRef::ColorRef(const Editor::ColorType type) :
        _Type{ type }
    {
    }

    ColorRef::ColorRef(const Core::Color& color) :
        _Color{ color },
        _Type{ ColorType::RGB }
    {
    }

    hstring ColorRef::ToString()
    {
        // TODO CARLOS: Localize!
        switch (_Type)
        {
        case ColorType::RGB:
            return hstring{ _Color.ToHexString(true) };
        case ColorType::TerminalBackground:
            return { L"Terminal Background" };
        case ColorType::Accent:
            return { L"Accent Color" };
        case ColorType::MoreColors:
            return { L"More Colors..." };
        default:
            // TODO CARLOS: this shouldn't exist
            return { L"Unknown" };
        }
    }

    Windows::UI::Color ColorRef::Color() const
    {
        switch (_Type)
        {
        case ColorType::TerminalBackground:
            // TODO CARLOS: get the current background and return it here
            return Windows::UI::Colors::Black();
        case ColorType::Accent:
            // TODO CARLOS: get the current accent color and return it here
            //  we need to get the resource dictionary and "res.Lookup(accentColorKey)"
            //static const auto accentColorKey{ winrt::box_value(L"SystemAccentColor") };
            return Windows::UI::Colors::Blue();
        default:
            return _Color;
        }
    }

    void ColorRef::Color(const Windows::UI::Color& value)
    {
        _Color = value;
        _Type = ColorType::RGB;
    }

    ColorPickerViewModel::ColorPickerViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings) :
        _profile{ profile },
        _settings{ settings },
        _colorList{ nullptr }
    {
    }

    Windows::Foundation::Collections::IObservableVector<Editor::ColorRef> ColorPickerViewModel::ColorList()
    {
        if (!_colorList)
        {
            _RefreshColorList();
        }
        return _colorList;
    }

    void ColorPickerViewModel::_RefreshColorList()
    {
        // TODO CARLOS: we're using default appearance and dark CS here,
        //               they should be parameters to the constructor, probably
        const auto schemeName = _profile.DefaultAppearance().DarkColorSchemeName();
        const auto scheme = _settings.GlobalSettings().ColorSchemes().Lookup(schemeName);

        const auto schemeTable = scheme.Table();
        auto colorList = single_threaded_observable_vector<Editor::ColorRef>();
        for (const auto& color : schemeTable)
        {
            colorList.Append(winrt::make<ColorRef>(color));
        }
        _colorList = colorList;
    }
}
