// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct HighlightedTextControl : HighlightedTextControlT<HighlightedTextControl>
    {
        HighlightedTextControl();

        void OnApplyTemplate();

        DEPENDENCY_PROPERTY(winrt::hstring, Text);
        DEPENDENCY_PROPERTY(winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun>, HighlightedRuns);
        DEPENDENCY_PROPERTY(winrt::Windows::UI::Xaml::Style, TextBlockStyle);
        DEPENDENCY_PROPERTY(winrt::Windows::UI::Xaml::Style, HighlightedRunStyle);

    private:
        static void _InitializeProperties();
        static void _onPropertyChanged(const Windows::UI::Xaml::DependencyObject& o, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
        void _updateTextAndStyle();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(HighlightedTextControl);
}
