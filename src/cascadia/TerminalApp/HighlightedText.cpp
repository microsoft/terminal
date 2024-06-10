// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HighlightedText.h"
#include "HighlightedTextSegment.g.cpp"
#include "HighlightedText.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    HighlightedTextSegment::HighlightedTextSegment(const winrt::hstring& textSegment, bool isHighlighted) :
        _TextSegment(textSegment),
        _IsHighlighted(isHighlighted)
    {
    }

    HighlightedText::HighlightedText(const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment>& segments) :
        _Segments(segments)
    {
    }

    int HighlightedText::Weight() const
    {
        auto result = 0;
        auto isNextSegmentWordBeginning = true;

        for (const auto& segment : _Segments)
        {
            const auto& segmentText = segment.TextSegment();
            const auto segmentSize = segmentText.size();

            if (segment.IsHighlighted())
            {
                // Give extra point for each consecutive match
                result += (segmentSize <= 1) ? segmentSize : 1 + 2 * (segmentSize - 1);

                // Give extra point if this segment is at the beginning of a word
                if (isNextSegmentWordBeginning)
                {
                    result++;
                }
            }

            isNextSegmentWordBeginning = segmentSize > 0 && segmentText[segmentSize - 1] == L' ';
        }

        return result;
    }
}
