// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <cmark.h>

struct MarkdownToXaml
{
public:
    static winrt::Windows::UI::Xaml::Controls::RichTextBlock Convert(std::string_view markdownText, const winrt::hstring& baseUrl);

private:
    MarkdownToXaml(const winrt::hstring& baseUrl);

    winrt::hstring _baseUri{ L"" };

    winrt::Windows::UI::Xaml::Controls::RichTextBlock _root{};
    winrt::Windows::UI::Xaml::Documents::Run _lastRun{ nullptr };
    winrt::Windows::UI::Xaml::Documents::Span _lastSpan{ nullptr };
    winrt::Windows::UI::Xaml::Documents::Paragraph _lastParagraph{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Image _lastImage{ nullptr };

    int _indent = 0;
    int _blockQuoteDepth = 0;

    winrt::Windows::UI::Xaml::Documents::Paragraph _CurrentParagraph();
    winrt::Windows::UI::Xaml::Documents::Run _CurrentRun();
    winrt::Windows::UI::Xaml::Documents::Span _CurrentSpan();
    winrt::Windows::UI::Xaml::Documents::Run _NewRun();
    void _EndRun() noexcept;
    void _EndSpan() noexcept;
    void _EndParagraph() noexcept;

    winrt::Windows::UI::Xaml::Controls::TextBlock _makeDefaultTextBlock();

    void _RenderNode(cmark_node* node, cmark_event_type ev_type);
};
