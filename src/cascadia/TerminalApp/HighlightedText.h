// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextSegment.g.h"
#include "HighlightedText.g.h"

namespace winrt::TerminalApp::implementation
{
    struct HighlightedTextSegment : HighlightedTextSegmentT<HighlightedTextSegment>
    {
        HighlightedTextSegment() = default;
        HighlightedTextSegment(const winrt::hstring& text, bool isHighlighted);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, TextSegment, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsHighlighted, PropertyChanged.raise);
    };

    struct HighlightedText : HighlightedTextT<HighlightedText>
    {
        HighlightedText() = default;
        HighlightedText(const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment>& segments);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment>, Segments, PropertyChanged.raise);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(HighlightedTextSegment);
    BASIC_FACTORY(HighlightedText);
}
