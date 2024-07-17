// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CodeBlock.h"
#include "MarkdownToXaml.h"

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}
using namespace winrt;

// Bullet points used for unordered lists.
static const std::wstring bullets[]{
    L"• ",
    L"◦ ",
    L"▪ " // After this level, we'll keep using this one.
};

template<typename T>
static std::string_view textFromCmarkString(const T& s)
{
    return std::string_view{ (char*)s.data, (size_t)s.len };
}
static std::string_view textFromLiteral(const cmark_node* const node)
{
    return textFromCmarkString(node->as.literal);
}
static std::string_view textFromUrl(const cmark_node* const node)
{
    return textFromCmarkString(node->as.link.url);
}

// Function Description:
// - Entrypoint to convert a string of markdown into a XAML RichTextBlock.
// Arguments:
// - markdownText: the markdown content to render
// - baseUrl: the current URI of the content. This will allow for relative links
//   to be appropriately resolved.
// Return Value:
// - a RichTextBlock with the rendered markdown in it.
WUX::Controls::RichTextBlock MarkdownToXaml::Convert(std::string_view markdownText, const winrt::hstring& baseUrl)
{
    MarkdownToXaml data{ baseUrl };

    auto doc = cmark_parse_document(markdownText.data(), markdownText.size(), CMARK_OPT_DEFAULT);
    auto iter = cmark_iter_new(doc);
    cmark_event_type ev_type;
    cmark_node* curr;

    while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE)
    {
        curr = cmark_iter_get_node(iter);
        data._RenderNode(curr, ev_type);
    }

    return data._root;
}

MarkdownToXaml::MarkdownToXaml(const winrt::hstring& baseUrl) :
    _baseUri{ baseUrl }
{
    _root.IsTextSelectionEnabled(true);
    _root.TextWrapping(WUX::TextWrapping::WrapWholeWords);
}

WUX::Documents::Paragraph MarkdownToXaml::_CurrentParagraph()
{
    if (_lastParagraph == nullptr)
    {
        _EndRun(); // sanity check
        _lastParagraph = WUX::Documents::Paragraph();
        if (_indent > 0)
        {
            if (_indent - _blockQuoteDepth > 0)
            {
                _lastParagraph.TextIndent(-12);
            }
            _lastParagraph.Margin(WUX::ThicknessHelper::FromLengths(18 * _indent, 0, 0, 0));
        }
        _root.Blocks().Append(_lastParagraph);
    }
    return _lastParagraph;
}
WUX::Documents::Run MarkdownToXaml::_CurrentRun()
{
    if (_currentRun == nullptr)
    {
        _currentRun = WUX::Documents::Run();
        _CurrentSpan().Inlines().Append(_currentRun);
    }
    return _currentRun;
}
WUX::Documents::Span MarkdownToXaml::_CurrentSpan()
{
    if (_currentSpan == nullptr)
    {
        _currentSpan = WUX::Documents::Span();
        _CurrentParagraph().Inlines().Append(_currentSpan);
    }
    return _currentSpan;
}
WUX::Documents::Run MarkdownToXaml::_NewRun()
{
    if (_currentRun == nullptr)
    {
        _currentRun = WUX::Documents::Run();
        _CurrentSpan().Inlines().Append(_currentRun);
    }
    else
    {
        auto old{ _currentRun };

        auto old_FontFamily = old.FontFamily();
        auto old_FontWeight = old.FontWeight();
        auto old_FontStyle = old.FontStyle();

        WUX::Documents::Run newRun{};

        newRun.FontFamily(old_FontFamily);
        newRun.FontWeight(old_FontWeight);
        newRun.FontStyle(old_FontStyle);

        _currentRun = newRun;
        _CurrentSpan().Inlines().Append(_currentRun);
    }
    return _currentRun;
}
void MarkdownToXaml::_EndRun()
{
    _currentRun = nullptr;
}
void MarkdownToXaml::_EndSpan()
{
    _EndRun();
    _currentSpan = nullptr;
}
void MarkdownToXaml::_EndParagraph()
{
    _EndSpan();
    _lastParagraph = nullptr;
}

WUX::Controls::TextBlock MarkdownToXaml::_makeDefaultTextBlock()
{
    WUX::Controls::TextBlock b{};
    b.IsTextSelectionEnabled(true);
    b.TextWrapping(WUX::TextWrapping::WrapWholeWords);
    return b;
}

void MarkdownToXaml::_RenderNode(cmark_node* node, cmark_event_type ev_type)
{
    cmark_node* parent;
    cmark_node* grandparent;
    bool tight;

    bool entering = (ev_type == CMARK_EVENT_ENTER);

    switch (node->type)
    {
    case CMARK_NODE_DOCUMENT:
        break;

    case CMARK_NODE_BLOCK_QUOTE:

        // It's non-trivial to deal with the right-side vertical lines that
        // we're accustomed to seeing for block quotes in markdown content.
        // RichTextBlock doesn't have a good way of adding a border to a
        // paragraph, it would seem.
        //
        // We could add a InlineUIContainer, with a Border in there, then
        // put a new RichTextBlock in there, but I believe text selection
        // wouldn't transit across the border.

        // Instead, we're just going to add a new layer of indenting.

        if (entering)
        {
            _EndParagraph();
            _indent++;
            _blockQuoteDepth++;
        }
        else
        {
            _EndParagraph();
            _indent = std::max(0, _indent - 1);
            _blockQuoteDepth = std::max(0, _blockQuoteDepth - 1);
        }

        break;

    case CMARK_NODE_LIST:
    {
        // when `node->as.list.list_type == CMARK_BULLET_LIST`, we're an unordered list.
        // Otherwise, we're an ordered one (and we might not start at 0).
        //
        // However, we don't support numbered lists for now.
        if (entering)
        {
            _EndParagraph();
            _indent++;
        }
        else
        {
            _EndParagraph();
            _indent = std::max(0, _indent - 1);
        }
        break;
    }

    case CMARK_NODE_ITEM:
        // A list item, either for a ordered list or an unordered one.
        if (entering)
        {
            _EndParagraph();
            _CurrentParagraph();
            _NewRun().Text(winrt::hstring{ bullets[std::clamp(_indent - _blockQuoteDepth - 1, 0, 2)] });
        }
        break;

    case CMARK_NODE_HEADING:
        _EndParagraph();

        // At the start of a header, change the font size to match the new
        // level of header we're at. The text will come later, in a
        // CMARK_NODE_TEXT
        if (entering)
        {
            const auto level = node->as.heading.level;
            _CurrentParagraph().FontSize(std::max(16u, 36u - ((level - 1) * 6u)));
        }
        break;

    case CMARK_NODE_CODE_BLOCK:
    {
        _EndParagraph();

        std::string_view code{ (char*)node->as.code.literal.data, (size_t)node->as.code.literal.len - 1 };
        const auto codeHstring{ winrt::hstring{ til::u8u16(code) } };

        auto codeBlock = winrt::make<winrt::TerminalApp::implementation::CodeBlock>(codeHstring);
        // codeBlock.RequestRunCommands({ page, &MarkdownPaneContent::_handleRunCommandRequest });
        WUX::Documents::InlineUIContainer codeContainer{};
        codeContainer.Child(codeBlock);
        _CurrentParagraph().Inlines().Append(codeContainer);

        _EndParagraph();
        _CurrentParagraph();
    }
    break;

    case CMARK_NODE_HTML_BLOCK:
        // Raw HTML comes to us in the literal
        //    node->as.literal.data, node->as.literal.len
        // But we don't support raw HTML, so we'll do nothing.
        break;

    case CMARK_NODE_CUSTOM_BLOCK:
        // Not even entirely sure what this is.
        break;

    case CMARK_NODE_THEMATIC_BREAK:
        // cmark_html_render_cr(html);
        // cmark_strbuf_puts(html, "<hr");
        // cmark_html_render_sourcepos(node, html, options);
        // cmark_strbuf_puts(html, " />\n");
        break;

    case CMARK_NODE_PARAGRAPH:
    {
        parent = cmark_node_parent(node);
        grandparent = cmark_node_parent(parent);
        if (grandparent != NULL && grandparent->type == CMARK_NODE_LIST)
        {
            tight = grandparent->as.list.tight;
        }
        else
        {
            tight = false;
        }

        // If we aren't in a list, then end the current paragraph and
        // start a new one.
        if (!tight)
        {
            _EndParagraph();
        }

        // Start a new paragraph if we don't have one
        _CurrentParagraph();
        break;
    }
    case CMARK_NODE_TEXT:
    {
        const auto text{ winrt::hstring{ til::u8u16(textFromLiteral(node)) } };

        if (_currentImage)
        {
            // The tooltip for an image comes in as a CMARK_NODE_TEXT, so set that here.
            WUX::Controls::ToolTipService::SetToolTip(_currentImage, box_value(text));
        }
        else
        {
            // Otherwise, just add the text to the current paragraph
            _NewRun().Text(text);
        }
    }

    break;

    case CMARK_NODE_LINEBREAK:
        _EndSpan();
        _CurrentParagraph().Inlines().Append(WUX::Documents::LineBreak());
        break;

    case CMARK_NODE_SOFTBREAK:
        // I'm fairly confident this is what happens when you've just got
        // two lines only seperated by a single \r\n in a MD doc. E.g. when
        // you want a paragraph to wrap at 80 columns in code, but wrap in
        // the rendered document.
        //
        // In the HTML implementation, what happens here depends on the options:
        // * CMARK_OPT_HARDBREAKS: add a full linebreak
        // * CMARK_OPT_NOBREAKS: Just add a space
        // * otherwise, just add a '\n'
        //
        // We're not really messing with options here, so lets just add a
        // space. That seems to keep the current line going, but allow for
        // word breaking.

        _NewRun().Text(L" ");
        break;

    case CMARK_NODE_CODE:
    {
        const auto text{ winrt::hstring{ til::u8u16(textFromLiteral(node)) } };
        const auto& codeRun{ _NewRun() };

        codeRun.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
        // A Span can't have a border or a background, so we can't give
        // it the whole treatment that a <code> span gets in HTML.
        codeRun.Text(text);

        _NewRun().FontFamily(_root.FontFamily());
    }
    break;

    case CMARK_NODE_HTML_INLINE:
        // Same as above - no raw HTML support here.
        break;

    case CMARK_NODE_CUSTOM_INLINE:
        // Same as above - not even entirely sure what this is.
        break;

    case CMARK_NODE_STRONG:
        _NewRun().FontWeight(entering ?
                                 winrt::Windows::UI::Text::FontWeights::Bold() :
                                 winrt::Windows::UI::Text::FontWeights::Normal());
        break;

    case CMARK_NODE_EMPH:
        _NewRun().FontStyle(entering ?
                                winrt::Windows::UI::Text::FontStyle::Italic :
                                winrt::Windows::UI::Text::FontStyle::Normal);
        break;

    case CMARK_NODE_LINK:
        // if (entering) {
        //   cmark_strbuf_puts(html, "<a href=\"");
        //   if ((options & CMARK_OPT_UNSAFE) ||
        //         !(scan_dangerous_url(&node->as.link.url, 0))) {
        //     houdini_escape_href(html, node->as.link.url.data,
        //                         node->as.link.url.len);
        //   }
        //   if (node->as.link.title.len) {
        //     cmark_strbuf_puts(html, "\" title=\"");
        //     escape_html(html, node->as.link.title.data, node->as.link.title.len);
        //   }
        //   cmark_strbuf_puts(html, "\">");
        // } else {
        //   cmark_strbuf_puts(html, "</a>");
        // }

        if (entering)
        {
            const auto url{ textFromUrl(node) };
            // std::string_view url{ (char*)node->as.link.url.data, (size_t)node->as.link.url.len };
            // std::string_view text{ (char*)node->as.link.title.data, (size_t)node->as.link.title.len };
            const auto urlHstring{ winrt::hstring{ til::u8u16(url) } };
            // TODO! add tooltip that does the unescaped URL thing that we do for termcontrol
            WUX::Documents::Hyperlink a{};
            winrt::Windows::Foundation::Uri uri{ _baseUri, urlHstring };
            a.NavigateUri(uri);
            _CurrentParagraph().Inlines().Append(a);
            _currentSpan = a;

            // Similar to the header element, the actual text of the link
            // will later come through as a CMARK_NODE_TEXT
        }
        else
        {
            _EndSpan();
        }
        break;

    case CMARK_NODE_IMAGE:
        if (entering)
        {
            std::string_view url{ (char*)node->as.link.url.data, (size_t)node->as.link.url.len };
            const auto urlHstring{ winrt::hstring{ til::u8u16(url) } };
            winrt::Windows::Foundation::Uri uri{ _baseUri, urlHstring };
            WUX::Controls::Image img{};
            WUX::Media::Imaging::BitmapImage bitmapImage;
            bitmapImage.UriSource(uri);
            img.Source(bitmapImage);
            WUX::Documents::InlineUIContainer imageBlock{};
            imageBlock.Child(img);
            _CurrentParagraph().Inlines().Append(imageBlock);
            _currentImage = img;
        }
        else
        {
            _EndSpan();
            _currentImage = nullptr;
        }
        break;

    case CMARK_NODE_FOOTNOTE_DEFINITION:
        // Not suppoorted currently
        break;

    case CMARK_NODE_FOOTNOTE_REFERENCE:
        // Not suppoorted currently
        break;

    default:
        assert(false);
        break;
    }
}