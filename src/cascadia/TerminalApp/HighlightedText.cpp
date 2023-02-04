// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HighlightedText.h"
#include "HighlightedTextSegment.g.cpp"
#include "HighlightedText.g.cpp"

using namespace winrt;
using namespace MTApp;
using namespace WUC;
using namespace WUX;
using namespace WS;
using namespace WUT;
using namespace WF;
using namespace WFC;
using namespace MTSM;

namespace winrt::TerminalApp::implementation
{
    HighlightedTextSegment::HighlightedTextSegment(const winrt::hstring& textSegment, bool isHighlighted) :
        _TextSegment(textSegment),
        _IsHighlighted(isHighlighted)
    {
    }

    HighlightedText::HighlightedText(const WFC::IObservableVector<MTApp::HighlightedTextSegment>& segments) :
        _Segments(segments)
    {
    }
}
