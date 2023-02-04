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

        static WUX::DependencyProperty TextProperty();

        MTApp::HighlightedText Text();
        void Text(const MTApp::HighlightedText& value);

        WUXC::TextBlock TextView();

    private:
        static WUX::DependencyProperty _textProperty;
        static void _onTextChanged(const WUX::DependencyObject& o, const WUX::DependencyPropertyChangedEventArgs& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(HighlightedTextControl);
}
