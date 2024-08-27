// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorRef.g.h"
#include "ColorPickerViewModel.g.h"
#include "TerminalColorToBrushConverter.g.h"
#include "TerminalColorToStringConverter.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct TerminalColorToBrushConverter : TerminalColorToBrushConverterT<TerminalColorToBrushConverter>
    {
        TerminalColorToBrushConverter() = default;

        Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language);
        Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language);
    };

    struct TerminalColorToStringConverter : TerminalColorToStringConverterT<TerminalColorToStringConverter>
    {
        TerminalColorToStringConverter() = default;

        Windows::Foundation::IInspectable Convert(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language);
        Windows::Foundation::IInspectable ConvertBack(Windows::Foundation::IInspectable const& value, Windows::UI::Xaml::Interop::TypeName const& targetType, Windows::Foundation::IInspectable const& parameter, hstring const& language);
    };

    struct ColorRef : ColorRefT<ColorRef>
    {
        ColorRef(const Windows::UI::Color& color);
        ColorRef(const Editor::ColorType type);
        ColorRef(const Core::Color& color);

        hstring ToString();

        hstring Name() { return ToString(); };

        Windows::UI::Color Color() const;
        void Color(const Windows::UI::Color& value);

        WINRT_PROPERTY(Editor::ColorType, Type);

    private:
        til::color _Color;
    };

    struct ColorPickerViewModel : ColorPickerViewModelT<ColorPickerViewModel>, ViewModelHelper<ColorPickerViewModel>
    {
    public:
        ColorPickerViewModel(const Model::Profile& profile, const Model::CascadiaSettings& settings);

        Windows::Foundation::Collections::IObservableVector<Editor::ColorRef> ColorList();
        WINRT_PROPERTY(Editor::ColorRef, CurrentColor, nullptr);

    private:
        Model::Profile _profile;
        Model::CascadiaSettings _settings;
        Windows::Foundation::Collections::IObservableVector<Editor::ColorRef> _colorList;

        void _RefreshColorList();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(TerminalColorToBrushConverter);
    BASIC_FACTORY(TerminalColorToStringConverter);
}
