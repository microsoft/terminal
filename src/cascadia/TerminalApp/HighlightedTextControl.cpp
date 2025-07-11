// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "HighlightedTextControl.h"

#include "HighlightedTextControl.g.cpp"

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
    // Our control exposes a "Text" property to be used with Data Binding
    // To allow this we need to register a Dependency Property Identifier to be used by the property system
    // (https://docs.microsoft.com/en-us/windows/uwp/xaml-platform/custom-dependency-properties)
    DependencyProperty HighlightedTextControl::_TextBlockStyleProperty{ nullptr };
    DependencyProperty HighlightedTextControl::_TextProperty{ nullptr };
    DependencyProperty HighlightedTextControl::_HighlightedRunsProperty{ nullptr };

    HighlightedTextControl::HighlightedTextControl()
    {
        _InitializeProperties();
    }

    void HighlightedTextControl::_InitializeProperties()
    {
        static auto [[maybe_unused]] registered = [] {
            _TextProperty = DependencyProperty::Register(
                L"Text",
                xaml_typename<winrt::hstring>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata(nullptr, HighlightedTextControl::_onPropertyChanged));

            _TextBlockStyleProperty = DependencyProperty::Register(
                L"TextBlockStyle",
                xaml_typename<winrt::Windows::UI::Xaml::Style>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata{ nullptr });

            _HighlightedRunsProperty = DependencyProperty::Register(
                L"HighlightedRuns",
                xaml_typename<winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun>>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata(nullptr, HighlightedTextControl::_onPropertyChanged));

            return true;
        }();
    }

    void HighlightedTextControl::_onPropertyChanged(const DependencyObject& o, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto control = o.try_as<winrt::TerminalApp::HighlightedTextControl>();
        if (control)
        {
            winrt::get_self<HighlightedTextControl>(control)->_updateTextAndStyle();
        }
    }

    void HighlightedTextControl::OnApplyTemplate()
    {
        _updateTextAndStyle();
    }

    void HighlightedTextControl::_updateTextAndStyle()
    {
        const auto textBlock = GetTemplateChild(L"TextView").try_as<winrt::Windows::UI::Xaml::Controls::TextBlock>();
        if (!textBlock)
        {
            return;
        }

        const auto text = Text();
        const auto runs = HighlightedRuns();

        // Replace all the runs on the TextBlock
        // Use the runs to decide if the run should be highlighted.
        // To do - export the highlighting style into XAML
        const auto inlinesCollection = textBlock.Inlines();
        inlinesCollection.Clear();

        if (!text.empty())
        {
            size_t lastPos = 0;
            if (runs && runs.Size())
            {
                for (const auto& [start, end] : runs)
                {
                    if (start > lastPos)
                    {
                        hstring nonMatch{ til::safe_slice_abs(text, lastPos, start) };
                        Documents::Run run;
                        run.Text(nonMatch);
                        run.FontWeight(FontWeights::Normal());
                        inlinesCollection.Append(run);
                    }

                    hstring matchSeg{ til::safe_slice_abs(text, start, end + 1) };
                    Documents::Run run;
                    run.Text(matchSeg);
                    run.FontWeight(FontWeights::Bold());
                    inlinesCollection.Append(run);

                    lastPos = end + 1;
                }
            }

            // This will also be true if there are no runs at all
            if (lastPos < text.size())
            {
                // checking lastPos here prevents a needless deep copy of the whole text in the no-match case
                hstring tail{ lastPos == 0 ? text : hstring{ til::safe_slice_abs(text, lastPos, SIZE_T_MAX) } };
                Documents::Run run;
                run.Text(tail);
                run.FontWeight(FontWeights::Normal());
                inlinesCollection.Append(run);
            }
        }
    }
}
