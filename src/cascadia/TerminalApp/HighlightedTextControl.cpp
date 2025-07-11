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

    HighlightedTextControl::HighlightedTextControl()
    {
        _InitializeProperties();
    }

    void HighlightedTextControl::_InitializeProperties()
    {
        static auto [[maybe_unused]] registered = [] {
            _TextProperty = DependencyProperty::Register(
                L"Text",
                xaml_typename<winrt::TerminalApp::HighlightedText>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata(nullptr, HighlightedTextControl::_onPropertyChanged));

            _TextBlockStyleProperty = DependencyProperty::Register(
                L"TextBlockStyle",
                xaml_typename<winrt::Windows::UI::Xaml::Style>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata{ nullptr });

            return true;
        }();
    }

    // Method Description:
    // - This callback is triggered when the Text property is changed. Responsible for updating the view
    // Arguments:
    // - o - dependency object that was modified, expected to be an instance of this control
    // - e - event arguments of the property changed event fired by the event system upon Text property change.
    // The new value is expected to be an instance of HighlightedText
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

        const auto highlightedText = Text();

        // Replace all the runs on the TextBlock
        // Use IsHighlighted to decide if the run should be highlighted.
        // To do - export the highlighting style into XAML
        const auto inlinesCollection = textBlock.Inlines();
        inlinesCollection.Clear();

        if (highlightedText)
        {
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
        else
        {
        }
    }
}
