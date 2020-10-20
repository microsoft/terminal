// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "HighlightedTextControl.h"

#include "HighlightedTextControl.g.cpp"
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
    HighlightedTextSegment::HighlightedTextSegment(winrt::hstring const& textSegment, bool isHighlighted) :
        _TextSegment(textSegment),
        _IsHighlighted(isHighlighted)
    {
    }

    HighlightedText::HighlightedText(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::HighlightedTextSegment> const& segments) :
        _Segments(segments)
    {
    }

    DependencyProperty HighlightedTextControl::_textProperty = DependencyProperty::Register(
        L"Text",
        xaml_typename<winrt::TerminalApp::HighlightedText>(),
        xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
        PropertyMetadata(nullptr, HighlightedTextControl::_onTextChanged));

    HighlightedTextControl::HighlightedTextControl()
    {
        InitializeComponent();
    }

    DependencyProperty HighlightedTextControl::TextProperty()
    {
        return _textProperty;
    }

    Controls::TextBlock HighlightedTextControl::TextView()
    {
        return _textView();
    }

    winrt::TerminalApp::HighlightedText HighlightedTextControl::Text()
    {
        return winrt::unbox_value<winrt::TerminalApp::HighlightedText>(GetValue(_textProperty));
    }

    void HighlightedTextControl::Text(winrt::TerminalApp::HighlightedText const& value)
    {
        SetValue(_textProperty, winrt::box_value(value));
    }

    void HighlightedTextControl::_onTextChanged(DependencyObject const& o, DependencyPropertyChangedEventArgs const& e)
    {
        const auto control = o.try_as<winrt::TerminalApp::HighlightedTextControl>();
        const auto highlightedText = e.NewValue().try_as<winrt::TerminalApp::HighlightedText>();

        if (control)
        {
            // Replace all the runs on the TextBlock
            // Use IsHighlighted to decide if the run should be hihglighted.
            // To do - export the highlighting style into XAML
            const auto inlinesCollection = control.TextView().Inlines();
            inlinesCollection.Clear();

            for (const auto& match : highlightedText.Segments())
            {
                const auto matchText = match.TextSegment();
                const auto fontWeight = match.IsHighlighted() ? FontWeights::Bold() : FontWeights::Normal();

                Documents::Run run;
                run.Text(matchText);
                run.FontWeight(fontWeight);
                inlinesCollection.Append(run);
            }
        }
    }
}
