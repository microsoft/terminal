#pragma once

#include <winrt/Windows.Foundation.h>
#include "FuzzySearchTextSegment.g.h"
#include "FuzzySearchTextLine.g.h"
#include "FuzzySearchResult.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct FuzzySearchTextSegment : FuzzySearchTextSegmentT<FuzzySearchTextSegment>
    {
        FuzzySearchTextSegment();
        FuzzySearchTextSegment(const winrt::hstring& textSegment, bool isHighlighted);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, TextSegment, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsHighlighted, _PropertyChangedHandlers);
    };

    struct FuzzySearchTextLine : FuzzySearchTextLineT<FuzzySearchTextLine>
    {
        FuzzySearchTextLine() = default;
        FuzzySearchTextLine(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::FuzzySearchTextSegment>& segments, Microsoft::Terminal::Core::Point firstPosition);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::FuzzySearchTextSegment>, Segments, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(Microsoft::Terminal::Core::Point, FirstPosition, _PropertyChangedHandlers)
    };

    struct FuzzySearchResult : FuzzySearchResultT<FuzzySearchResult>
    {
        FuzzySearchResult() = default;
        FuzzySearchResult(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>& results, int32_t totalRowsSearched, int32_t numberOfResults);

        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>, Results);
        WINRT_PROPERTY(int32_t, TotalRowsSearched);
        WINRT_PROPERTY(int32_t, NumberOfResults);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(FuzzySearchTextSegment);
    BASIC_FACTORY(FuzzySearchTextLine);
    BASIC_FACTORY(FuzzySearchResult);
}
