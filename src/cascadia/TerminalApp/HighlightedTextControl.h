// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "HighlightedTextControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct HighlightedTextControl : HighlightedTextControlT<HighlightedTextControl>
    {
        HighlightedTextControl();

        static Windows::UI::Xaml::DependencyProperty TextProperty();

        winrt::TerminalApp::HighlightedText Text();
        void Text(winrt::TerminalApp::HighlightedText const& value);

        Windows::UI::Xaml::Controls::TextBlock TextView();

    private:
        static Windows::UI::Xaml::DependencyProperty _textProperty;
        static void _onTextChanged(Windows::UI::Xaml::DependencyObject const& o, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(HighlightedTextControl);
}
