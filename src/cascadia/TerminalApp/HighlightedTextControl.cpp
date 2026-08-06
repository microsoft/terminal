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
    DependencyProperty HighlightedTextControl::_TextProperty{ nullptr };
    DependencyProperty HighlightedTextControl::_HighlightedRunsProperty{ nullptr };
    DependencyProperty HighlightedTextControl::_TextBlockStyleProperty{ nullptr };
    DependencyProperty HighlightedTextControl::_HighlightedRunStyleProperty{ nullptr };

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

            _HighlightedRunsProperty = DependencyProperty::Register(
                L"HighlightedRuns",
                xaml_typename<winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun>>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata(nullptr, HighlightedTextControl::_onPropertyChanged));

            _TextBlockStyleProperty = DependencyProperty::Register(
                L"TextBlockStyle",
                xaml_typename<winrt::Windows::UI::Xaml::Style>(),
                xaml_typename<winrt::TerminalApp::HighlightedTextControl>(),
                PropertyMetadata{ nullptr });

            _HighlightedRunStyleProperty = DependencyProperty::Register(
                L"HighlightedRunStyle",
                xaml_typename<winrt::Windows::UI::Xaml::Style>(),
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

    static void _applyStyleToObject(const winrt::Windows::UI::Xaml::Style& style, const winrt::Windows::UI::Xaml::DependencyObject& object)
    {
        if (!style)
        {
            return;
        }

        static const auto fontWeightProperty{ winrt::Windows::UI::Xaml::Documents::TextElement::FontWeightProperty() };

        const auto setters{ style.Setters() };
        for (auto&& setterBase : setters)
        {
            const auto setter = setterBase.as<winrt::Windows::UI::Xaml::Setter>();
            const auto property = setter.Property();
            auto value = setter.Value();

            if (property == fontWeightProperty) [[unlikely]]
            {
                // BODGY - The XAML compiler emits a boxed int32, but the dependency property
                // here expects a boxed FontWeight (which also requires a u16. heh.)
                // FontWeight is one of the few properties that is broken like this, and on Run it's the
                // only one... so we can trivially check this case.
                const auto weight{ winrt::unbox_value_or<int32_t>(value, static_cast<int32_t>(400)) };
                value = winrt::box_value(winrt::Windows::UI::Text::FontWeight{ static_cast<uint16_t>(weight) });
            }

            object.SetValue(property, value);
        }
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

        const auto inlinesCollection = textBlock.Inlines();
        inlinesCollection.Clear();

        // The code below constructs local hstring instances because hstring is required to be null-terminated
        // and slicing _does not_ guarantee null termination. Passing a sliced wstring_view directly into run.Text()
        // (which is a winrt::param::hstring--different thing!--will result in an exception when the sliced portion
        // is not null-terminated.
        if (!text.empty())
        {
            size_t lastPos = 0;
            if (runs && runs.Size())
            {
                const auto runStyle = HighlightedRunStyle();

                for (const auto& [start, end] : runs)
                {
                    if (start > lastPos)
                    {
                        const hstring nonMatch{ til::safe_slice_abs(text, lastPos, static_cast<size_t>(start)) };
                        Documents::Run run;
                        run.Text(nonMatch);
                        inlinesCollection.Append(run);
                    }

                    const hstring matchSeg{ til::safe_slice_abs(text, static_cast<size_t>(start), static_cast<size_t>(end + 1)) };
                    Documents::Run run;
                    run.Text(matchSeg);

                    if (runStyle) [[unlikely]]
                    {
                        _applyStyleToObject(runStyle, run);
                    }
                    else
                    {
                        // Default style: bold
                        run.FontWeight(FontWeights::Bold());
                    }
                    inlinesCollection.Append(run);

                    lastPos = static_cast<size_t>(end + 1);
                }
            }

            // This will also be true if there are no runs at all
            if (lastPos < text.size())
            {
                // checking lastPos here prevents a needless deep copy of the whole text in the no-match case
                const hstring tail{ lastPos == 0 ? text : hstring{ til::safe_slice_abs(text, lastPos, SIZE_T_MAX) } };
                Documents::Run run;
                run.Text(tail);
                inlinesCollection.Append(run);
            }
        }
    }
}
