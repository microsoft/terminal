// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "HighlightedTextControl.g.h"

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct HighlightedTextControl : HighlightedTextControlT<HighlightedTextControl>
    {
        HighlightedTextControl();

        static Windows::UI::Xaml::DependencyProperty TextProperty();

        winrt::Microsoft::Terminal::App::HighlightedText Text();
        void Text(const winrt::Microsoft::Terminal::App::HighlightedText& value);

        Windows::UI::Xaml::Controls::TextBlock TextView();

    private:
        static Windows::UI::Xaml::DependencyProperty _textProperty;
        static void _onTextChanged(const Windows::UI::Xaml::DependencyObject& o, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(HighlightedTextControl);
}
