// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "HighlightedTextSegment.g.h"
#include "HighlightedText.g.h"
#include "HighlightedTextControl.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct HighlightedTextSegment : HighlightedTextSegmentT<HighlightedTextSegment>
    {
        HighlightedTextSegment() = default;
        HighlightedTextSegment(winrt::hstring const& text, bool isHighlighted);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, TextSegment, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsHighlighted, _PropertyChangedHandlers);
    };

    struct HighlightedText : HighlightedTextT<HighlightedText>
    {
        HighlightedText() = default;
        HighlightedText(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment> const& segments);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment>, Segments, _PropertyChangedHandlers);
    };

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
    BASIC_FACTORY(HighlightedTextSegment);
    BASIC_FACTORY(HighlightedText);
    BASIC_FACTORY(HighlightedTextControl);
}
