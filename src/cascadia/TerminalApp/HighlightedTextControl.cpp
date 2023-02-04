// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "HighlightedTextControl.h"

#include "HighlightedTextControl.g.cpp"

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
    // Our control exposes a "Text" property to be used with Data Binding
    // To allow this we need to register a Dependency Property Identifier to be used by the property system
    // (https://docs.microsoft.com/en-us/windows/uwp/xaml-platform/custom-dependency-properties)
    DependencyProperty HighlightedTextControl::_textProperty = DependencyProperty::Register(
        L"Text",
        xaml_typename<MTApp::HighlightedText>(),
        xaml_typename<MTApp::HighlightedTextControl>(),
        PropertyMetadata(nullptr, HighlightedTextControl::_onTextChanged));

    HighlightedTextControl::HighlightedTextControl()
    {
        InitializeComponent();
    }

    // Method Description:
    // - Returns the Identifier of the "Text" dependency property
    DependencyProperty HighlightedTextControl::TextProperty()
    {
        return _textProperty;
    }

    // Method Description:
    // - Returns the TextBlock view used to render the highlighted text
    // Can be used when the Text property change is triggered by the event system to update the view
    // We need to expose it rather than simply bind a data source because we update the runs in code-behind
    Controls::TextBlock HighlightedTextControl::TextView()
    {
        return _textView();
    }

    MTApp::HighlightedText HighlightedTextControl::Text()
    {
        return winrt::unbox_value<MTApp::HighlightedText>(GetValue(_textProperty));
    }

    void HighlightedTextControl::Text(const MTApp::HighlightedText& value)
    {
        SetValue(_textProperty, winrt::box_value(value));
    }

    // Method Description:
    // - This callback is triggered when the Text property is changed. Responsible for updating the view
    // Arguments:
    // - o - dependency object that was modified, expected to be an instance of this control
    // - e - event arguments of the property changed event fired by the event system upon Text property change.
    // The new value is expected to be an instance of HighlightedText
    void HighlightedTextControl::_onTextChanged(const DependencyObject& o, const DependencyPropertyChangedEventArgs& e)
    {
        const auto control = o.try_as<MTApp::HighlightedTextControl>();
        const auto highlightedText = e.NewValue().try_as<MTApp::HighlightedText>();

        if (control && highlightedText)
        {
            // Replace all the runs on the TextBlock
            // Use IsHighlighted to decide if the run should be highlighted.
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
