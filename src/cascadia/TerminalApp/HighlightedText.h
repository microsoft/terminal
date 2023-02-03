// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextSegment.g.h"
#include "HighlightedText.g.h"

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct HighlightedTextSegment : HighlightedTextSegmentT<HighlightedTextSegment>
    {
        HighlightedTextSegment() = default;
        HighlightedTextSegment(const winrt::hstring& text, bool isHighlighted);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, TextSegment, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsHighlighted, _PropertyChangedHandlers);
    };

    struct HighlightedText : HighlightedTextT<HighlightedText>
    {
        HighlightedText() = default;
        HighlightedText(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::App::HighlightedTextSegment>& segments);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::App::HighlightedTextSegment>, Segments, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(HighlightedTextSegment);
    BASIC_FACTORY(HighlightedText);
}
