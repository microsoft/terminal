// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MarkdownPaneContent.h"
#include <LibraryResources.h>
#include "MarkdownPaneContent.g.cpp"
#include "CodeBlock.h"

#include "../../oss/cmark-gfm/src/cmark-gfm.h"
#include "../../oss/cmark-gfm/src/node.h"

// #define MD4C_USE_UTF16
// #include "..\..\oss\md4c\md4c.h"

using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    struct MyMarkdownData
    {
        WUX::Controls::RichTextBlock root{};
        // implementation::MarkdownPaneContent* page{ nullptr };
        winrt::hstring baseUri{ L"" };
        WUX::Controls::TextBlock current{ nullptr };
        WUX::Documents::Run _currentRun{ nullptr };
        WUX::Documents::Span _currentSpan{ nullptr };
        WUX::Documents::Paragraph lastParagraph{ nullptr };
        TerminalApp::CodeBlock currentCodeBlock{ nullptr };

        WUX::Documents::Paragraph currentParagraph()
        {
            if (lastParagraph == nullptr)
            {
                EndRun(); // sanity check
                lastParagraph = WUX::Documents::Paragraph();
                root.Blocks().Append(lastParagraph);
            }
            return lastParagraph;
        }
        WUX::Documents::Run currentRun()
        {
            if (_currentRun == nullptr)
            {
                _currentRun = WUX::Documents::Run();
                currentSpan().Inlines().Append(_currentRun);
            }
            return _currentRun;
        }
        WUX::Documents::Span currentSpan()
        {
            if (_currentSpan == nullptr)
            {
                _currentSpan = WUX::Documents::Span();
                currentParagraph().Inlines().Append(_currentSpan);
            }
            return _currentSpan;
        }
        WUX::Documents::Run newRun()
        {
            if (_currentRun == nullptr)
            {
                _currentRun = WUX::Documents::Run();
                currentSpan().Inlines().Append(_currentRun);
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
                // currentParagraph().Inlines().Append(_currentRun);
                currentSpan().Inlines().Append(_currentRun);
            }
            return _currentRun;
        }
        void EndRun()
        {
            _currentRun = nullptr;
        }
        void EndSpan()
        {
            EndRun();
            _currentSpan = nullptr;
        }
        void EndParagraph()
        {
            EndSpan();
            lastParagraph = nullptr;
        }
    };
    WUX::Controls::TextBlock makeDefaultTextBlock()
    {
        WUX::Controls::TextBlock b{};
        b.IsTextSelectionEnabled(true);
        b.TextWrapping(WUX::TextWrapping::WrapWholeWords);
        return b;
    }

    // int md_parser_enter_block(MD_BLOCKTYPE type, void* detail, void* userdata)
    // {
    //     MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
    //     switch (type)
    //     {
    //     case MD_BLOCK_UL:
    //     {
    //         break;
    //     }
    //     case MD_BLOCK_H:
    //     {
    //         MD_BLOCK_H_DETAIL* headerDetail = reinterpret_cast<MD_BLOCK_H_DETAIL*>(detail);
    //         data->current = makeDefaultTextBlock();
    //         const auto fontSize = std::max(16u, 36u - ((headerDetail->level - 1) * 6u));
    //         data->current.FontSize(fontSize);
    //         data->current.FontWeight(Windows::UI::Text::FontWeights::Bold());
    //         WUX::Documents::Run run{};
    //         // run.Text(winrtL'#');

    //         // Immediately add the header block
    //         data->root.Children().Append(data->current);

    //         if (headerDetail->level == 1)
    //         {
    //             // <Border Height="1" BorderThickness="1" BorderBrush="Red" HorizontalAlignment="Stretch"></Border>
    //             WUX::Controls::Border b;
    //             b.Height(1);
    //             b.BorderThickness(WUX::ThicknessHelper::FromLengths(1, 1, 1, 1));
    //             b.BorderBrush(WUX::Media::SolidColorBrush(Windows::UI::Colors::Gray()));
    //             b.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
    //             data->root.Children().Append(b);
    //         }
    //         break;
    //     }
    //     case MD_BLOCK_CODE:
    //     {
    //         MD_BLOCK_CODE_DETAIL* codeDetail = reinterpret_cast<MD_BLOCK_CODE_DETAIL*>(detail);
    //         codeDetail;

    //         data->currentCodeBlock = winrt::make<implementation::CodeBlock>(L"");
    //         data->currentCodeBlock.Margin(WUX::ThicknessHelper::FromLengths(8, 8, 8, 8));
    //         data->currentCodeBlock.RequestRunCommands({ data->page, &MarkdownPaneContent::_handleRunCommandRequest });

    //         data->root.Children().Append(data->currentCodeBlock);
    //     }
    //     default:
    //     {
    //         break;
    //     }
    //     }
    //     return 0;
    // }
    // int md_parser_leave_block(MD_BLOCKTYPE type, void* /*detail*/, void* userdata)
    // {
    //     MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
    //     data;
    //     switch (type)
    //     {
    //     case MD_BLOCK_UL:
    //     {
    //         break;
    //     }
    //     case MD_BLOCK_H:
    //     {
    //         // data->root.Children().Append(data->current);
    //         data->current = nullptr;
    //         break;
    //     }
    //     case MD_BLOCK_CODE:
    //     {
    //         // data->root.Children().Append(data->current);
    //         data->current = nullptr;
    //         break;
    //     }
    //     default:
    //     {
    //         break;
    //     }
    //     }
    //     return 0;
    // }
    // int md_parser_enter_span(MD_SPANTYPE type, void* /*detail*/, void* userdata)
    // {
    //     MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
    //     data;

    //     if (data->current == nullptr)
    //     {
    //         data->current = makeDefaultTextBlock();
    //         data->root.Children().Append(data->current);
    //     }
    //     if (data->currentRun == nullptr)
    //     {
    //         data->currentRun = WUX::Documents::Run();
    //     }
    //     auto currentRun = data->currentRun;
    //     switch (type)
    //     {
    //     case MD_SPAN_STRONG:
    //     {
    //         currentRun.FontWeight(Windows::UI::Text::FontWeights::Bold());
    //         break;
    //     }
    //     case MD_SPAN_EM:
    //     {
    //         currentRun.FontStyle(Windows::UI::Text::FontStyle::Italic);
    //         break;
    //     }
    //     case MD_SPAN_CODE:
    //     {
    //         currentRun.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
    //         break;
    //     }
    //     default:
    //     {
    //         break;
    //     }
    //     }
    //     return 0;
    // }
    // int md_parser_leave_span(MD_SPANTYPE type, void* /*detail*/, void* userdata)
    // {
    //     MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
    //     switch (type)
    //     {
    //     case MD_SPAN_EM:
    //     case MD_SPAN_STRONG:
    //     // {
    //     //     break;
    //     // }
    //     case MD_SPAN_CODE:
    //     {
    //         if (const auto& currentRun{ data->currentRun })
    //         {
    //             // data->current.Inlines().Append(currentRun);
    //             // data->currentRun = nullptr;
    //         }
    //         break;
    //     }
    //     default:
    //     {
    //         break;
    //     }
    //     }
    //     return 0;
    // }
    // int md_parser_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
    // {
    //     MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
    //     winrt::hstring str{ text, size };
    //     switch (type)
    //     {
    //     case MD_TEXT_BR:
    //     case MD_TEXT_SOFTBR:
    //     {
    //         if (const auto& curr{ data->current })
    //         {
    //             data->current = makeDefaultTextBlock();
    //             data->root.Children().Append(data->current);
    //         }

    //         break;
    //     }
    //     case MD_TEXT_CODE:
    //     {
    //         if (str == L"\n")
    //         {
    //             break;
    //         }
    //         if (const auto& codeBlock{ data->currentCodeBlock })
    //         {
    //             // code in a fenced block
    //             auto currentText = codeBlock.Commandlines();
    //             auto newText = currentText.empty() ? str :
    //                                                  currentText + winrt::hstring{ L"\r\n" } + str;
    //             codeBlock.Commandlines(newText);
    //             break;
    //         }
    //         else
    //         {
    //             // just normal `code` inline
    //             // data->currentRun.Text(str);
    //             [[fallthrough]];
    //         }
    //     }
    //     case MD_TEXT_NORMAL:
    //     default:
    //     {
    //         data->currentCodeBlock = nullptr;

    //         auto run = data->currentRun ? data->currentRun : WUX::Documents::Run{};
    //         run.Text(str);
    //         if (data->current)
    //         {
    //             data->current.Inlines().Append(run);
    //         }
    //         else
    //         {
    //             WUX::Controls::TextBlock block = makeDefaultTextBlock();
    //             block.Inlines().Append(run);
    //             data->root.Children().Append(block);
    //             data->current = block;
    //         }
    //         // data->root.Children().Append(block);

    //         data->currentRun = nullptr;
    //         break;
    //     }
    //     }
    //     return 0;
    // }

    // int parseMarkdown(const winrt::hstring& markdown, MyMarkdownData& data)
    // {
    //     MD_PARSER parser{
    //         .abi_version = 0,
    //         .flags = 0,
    //         .enter_block = &md_parser_enter_block,
    //         .leave_block = &md_parser_leave_block,
    //         .enter_span = &md_parser_enter_span,
    //         .leave_span = &md_parser_leave_span,
    //         .text = &md_parser_text,
    //     };

    //     const auto result = md_parse(
    //         markdown.c_str(),
    //         (unsigned)markdown.size(),
    //         &parser,
    //         &data // user data
    //     );

    //     return result;
    // }

    ////////////////////////////////////////////////////////////////////////////////
    void renderNode(cmark_node* node, cmark_event_type ev_type, MyMarkdownData& data, int /*options*/)
    {
        // cmark_node* parent;
        // cmark_node* grandparent;
        // cmark_strbuf *html = renderer->html;
        // cmark_llist* it;
        // cmark_syntax_extension* ext;
        // char start_heading[] = "<h0";
        // char end_heading[] = "</h0";
        // bool tight;
        // bool filtered;
        // char buffer[BUFFER_SIZE];

        bool entering = (ev_type == CMARK_EVENT_ENTER);

        // if (renderer->plain == node) { // back at original node
        //   renderer->plain = NULL;
        // }

        // if (renderer->plain != NULL) {
        //   switch (node->type) {
        //   case CMARK_NODE_TEXT:
        //   case CMARK_NODE_CODE:
        //   case CMARK_NODE_HTML_INLINE:
        //     escape_html(html, node->as.literal.data, node->as.literal.len);
        //     break;

        //   case CMARK_NODE_LINEBREAK:
        //   case CMARK_NODE_SOFTBREAK:
        //     cmark_strbuf_putc(html, ' ');
        //     break;

        //   default:
        //     break;
        //   }
        //   return 1;
        // }

        //if (node->extension && node->extension->html_render_func)
        //{
        //    // node->extension->html_render_func(node->extension, renderer, node, ev_type, options);
        //    return 1;
        //}

        switch (node->type)
        {
        case CMARK_NODE_DOCUMENT:
            break;

        case CMARK_NODE_BLOCK_QUOTE:
            // if (entering) {
            //   cmark_html_render_cr(html);
            //   cmark_strbuf_puts(html, "<blockquote");
            //   cmark_html_render_sourcepos(node, html, options);
            //   cmark_strbuf_puts(html, ">\n");
            // } else {
            //   cmark_html_render_cr(html);
            //   cmark_strbuf_puts(html, "</blockquote>\n");
            // }
            break;

        case CMARK_NODE_LIST:
        {
            // cmark_list_type list_type = node->as.list.list_type;
            // int start = node->as.list.start;

            // if (entering) {
            //   cmark_html_render_cr(html);
            //   if (list_type == CMARK_BULLET_LIST) {
            //     cmark_strbuf_puts(html, "<ul");
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_puts(html, ">\n");
            //   } else if (start == 1) {
            //     cmark_strbuf_puts(html, "<ol");
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_puts(html, ">\n");
            //   } else {
            //     snprintf(buffer, BUFFER_SIZE, "<ol start=\"%d\"", start);
            //     cmark_strbuf_puts(html, buffer);
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_puts(html, ">\n");
            //   }
            // } else {
            //   cmark_strbuf_puts(html,
            //                     list_type == CMARK_BULLET_LIST ? "</ul>\n" : "</ol>\n");
            // }
            break;
        }

        case CMARK_NODE_ITEM:
            // if (entering) {
            //   cmark_html_render_cr(html);
            //   cmark_strbuf_puts(html, "<li");
            //   cmark_html_render_sourcepos(node, html, options);
            //   cmark_strbuf_putc(html, '>');
            // } else {
            //   cmark_strbuf_puts(html, "</li>\n");
            // }
            break;

        case CMARK_NODE_HEADING:
            data.lastParagraph = nullptr;

            if (entering)
            {
                // auto heading = node->content;
                // auto headingHstr{ winrt::to_hstring(std::string_view{ (char*)heading.ptr, (size_t)heading.size }) };

                auto level = node->as.heading.level;

                auto paragraph{ data.currentParagraph() };
                paragraph.FontSize((double)(14 + 6 * (6 - level)));

                // WUX::Documents::Run run{};
                // auto currentRun{ data.currentRun() };
                // run.Text(headingHstr);
                // paragraph.Inlines().Append(run);
                // data.root.Blocks().Append(paragraph);
            }
            break;

        case CMARK_NODE_CODE_BLOCK:
            // cmark_html_render_cr(html);

            // if (node->as.code.info.len == 0) {
            //   cmark_strbuf_puts(html, "<pre");
            //   cmark_html_render_sourcepos(node, html, options);
            //   cmark_strbuf_puts(html, "><code>");
            // } else {
            //   bufsize_t first_tag = 0;
            //   while (first_tag < node->as.code.info.len &&
            //          !cmark_isspace(node->as.code.info.data[first_tag])) {
            //     first_tag += 1;
            //   }

            //   if (options & CMARK_OPT_GITHUB_PRE_LANG) {
            //     cmark_strbuf_puts(html, "<pre");
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_puts(html, " lang=\"");
            //     escape_html(html, node->as.code.info.data, first_tag);
            //     if (first_tag < node->as.code.info.len && (options & CMARK_OPT_FULL_INFO_STRING)) {
            //       cmark_strbuf_puts(html, "\" data-meta=\"");
            //       escape_html(html, node->as.code.info.data + first_tag + 1, node->as.code.info.len - first_tag - 1);
            //     }
            //     cmark_strbuf_puts(html, "\"><code>");
            //   } else {
            //     cmark_strbuf_puts(html, "<pre");
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_puts(html, "><code class=\"language-");
            //     escape_html(html, node->as.code.info.data, first_tag);
            //     if (first_tag < node->as.code.info.len && (options & CMARK_OPT_FULL_INFO_STRING)) {
            //       cmark_strbuf_puts(html, "\" data-meta=\"");
            //       escape_html(html, node->as.code.info.data + first_tag + 1, node->as.code.info.len - first_tag - 1);
            //     }
            //     cmark_strbuf_puts(html, "\">");
            //   }
            // }

            // escape_html(html, node->as.code.literal.data, node->as.code.literal.len);
            // cmark_strbuf_puts(html, "</code></pre>\n");
            break;

        case CMARK_NODE_HTML_BLOCK:
            // cmark_html_render_cr(html);
            // if (!(options & CMARK_OPT_UNSAFE)) {
            //   cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
            // }
            // else if (renderer->filter_extensions) {
            //   filter_html_block(renderer, node->as.literal.data, node->as.literal.len);
            // }
            // else {
            //   cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
            // }
            // cmark_html_render_cr(html);
            break;

        case CMARK_NODE_CUSTOM_BLOCK:
            // cmark_html_render_cr(html);
            // if (entering) {
            //   cmark_strbuf_put(html, node->as.custom.on_enter.data,
            //                    node->as.custom.on_enter.len);
            // } else {
            //   cmark_strbuf_put(html, node->as.custom.on_exit.data,
            //                    node->as.custom.on_exit.len);
            // }
            // cmark_html_render_cr(html);
            break;

        case CMARK_NODE_THEMATIC_BREAK:
            // cmark_html_render_cr(html);
            // cmark_strbuf_puts(html, "<hr");
            // cmark_html_render_sourcepos(node, html, options);
            // cmark_strbuf_puts(html, " />\n");
            break;

        case CMARK_NODE_PARAGRAPH:
        {
            // parent = cmark_node_parent(node);
            // grandparent = cmark_node_parent(parent);
            // if (grandparent != NULL && grandparent->type == CMARK_NODE_LIST) {
            //   tight = grandparent->as.list.tight;
            // } else {
            //   tight = false;
            // }
            // if (!tight) {
            //   if (entering) {
            //     cmark_html_render_cr(html);
            //     cmark_strbuf_puts(html, "<p");
            //     cmark_html_render_sourcepos(node, html, options);
            //     cmark_strbuf_putc(html, '>');
            //   } else {
            //     if (parent->type == CMARK_NODE_FOOTNOTE_DEFINITION && node->next == NULL) {
            //       cmark_strbuf_putc(html, ' ');
            //       S_put_footnote_backref(renderer, html, parent);
            //     }
            //     cmark_strbuf_puts(html, "</p>\n");
            //   }
            // }
            {
                data.EndParagraph();
                data.lastParagraph = WUX::Documents::Paragraph();
                data.root.Blocks().Append(data.lastParagraph);
            }
            break;
        }
        case CMARK_NODE_TEXT:
            // escape_html(html, node->as.literal.data, node->as.literal.len);
            {
                // WUX::Controls::TextBlock tb = makeDefaultTextBlock();
                // tb.Text(winrt::to_hstring(std::string_view{ (char*)node->as.literal.data, (size_t)node->as.literal.len }));
                // data.root.Children().Append(tb);

                // WUX::Documents::Run run{};
                const auto text{ winrt::to_hstring(std::string_view{ (char*)node->as.literal.data, (size_t)node->as.literal.len }) };
                data.newRun().Text(text);
                // data.currentParagraph().Inlines().Append(run);
            }

            break;

        case CMARK_NODE_LINEBREAK:
            // cmark_strbuf_puts(html, "<br />\n");
            data.EndSpan();
            data.currentParagraph().Inlines().Append(WUX::Documents::LineBreak());
            break;

        case CMARK_NODE_SOFTBREAK:
            // if (options & CMARK_OPT_HARDBREAKS) {
            //   cmark_strbuf_puts(html, "<br />\n");
            // } else if (options & CMARK_OPT_NOBREAKS) {
            //   cmark_strbuf_putc(html, ' ');
            // } else {
            //   cmark_strbuf_putc(html, '\n');
            // }
            data.newRun().Text(L" ");
            break;

        case CMARK_NODE_CODE:
            // cmark_strbuf_puts(html, "<code>");
            // escape_html(html, node->as.literal.data, node->as.literal.len);
            // cmark_strbuf_puts(html, "</code>");
            // if (entering)
            {
                const auto text{ winrt::to_hstring(std::string_view{ (char*)node->as.literal.data, (size_t)node->as.literal.len }) };

                const auto& codeRun{ data.newRun() };

                codeRun.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
                codeRun.Text(text);

                data.newRun().FontFamily(data.root.FontFamily());
            }
            // else
            // {
            // }
            break;

        case CMARK_NODE_HTML_INLINE:
            // if (!(options & CMARK_OPT_UNSAFE)) {
            //   cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
            // } else {
            //   filtered = false;
            //   for (it = renderer->filter_extensions; it; it = it->next) {
            //     ext = (cmark_syntax_extension *) it->data;
            //     if (!ext->html_filter_func(ext, node->as.literal.data, node->as.literal.len)) {
            //       filtered = true;
            //       break;
            //     }
            //   }
            //   if (!filtered) {
            //     cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
            //   } else {
            //     cmark_strbuf_puts(html, "&lt;");
            //     cmark_strbuf_put(html, node->as.literal.data + 1, node->as.literal.len - 1);
            //   }
            // }
            break;

        case CMARK_NODE_CUSTOM_INLINE:
            // if (entering) {
            //   cmark_strbuf_put(html, node->as.custom.on_enter.data,
            //                    node->as.custom.on_enter.len);
            // } else {
            //   cmark_strbuf_put(html, node->as.custom.on_exit.data,
            //                    node->as.custom.on_exit.len);
            // }
            break;

        case CMARK_NODE_STRONG:
            // if (node->parent == NULL || node->parent->type != CMARK_NODE_STRONG) {
            //   if (entering) {
            //     cmark_strbuf_puts(html, "<strong>");
            //   } else {
            //     cmark_strbuf_puts(html, "</strong>");
            //   }
            // }
            if (entering)
            {
                data.newRun().FontWeight(Windows::UI::Text::FontWeights::Bold());
            }
            else
            {
                // data.EndRun();
                data.newRun().FontWeight(Windows::UI::Text::FontWeights::Normal());
            }
            break;

        case CMARK_NODE_EMPH:
            if (entering)
            {
                data.newRun().FontStyle(Windows::UI::Text::FontStyle::Italic);
            }
            else
            {
                // data.EndRun();
                data.newRun().FontStyle(Windows::UI::Text::FontStyle::Normal);
            }
            // if (entering) {
            //   cmark_strbuf_puts(html, "<em>");
            // } else {
            //   cmark_strbuf_puts(html, "</em>");
            // }
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
                std::string_view url{ (char*)node->as.link.url.data, (size_t)node->as.link.url.len };
                // std::string_view text{ (char*)node->as.link.title.data, (size_t)node->as.link.title.len };
                const auto urlHstring{ winrt::to_hstring(url) };
                // const auto textHstring{ winrt::to_hstring(text) };

                // <Hyperlink NavigateUri="https://docs.microsoft.com/uwp/api/Windows.UI.Xaml.Documents.Hyperlink">hyperlinks</Hyperlink>
                WUX::Documents::Hyperlink a{};
                Windows::Foundation::Uri uri{ data.baseUri, urlHstring };
                a.NavigateUri(uri);
                // WUX::Documents::Run linkRun{};
                // linkRun.Text(textHstring);
                // a.Inlines().Append(linkRun);
                data.currentParagraph().Inlines().Append(a);
                data._currentSpan = a;
            }
            else
            {
                data.EndSpan();
            }
            break;

        case CMARK_NODE_IMAGE:
            // if (entering) {
            //   cmark_strbuf_puts(html, "<img src=\"");
            //   if ((options & CMARK_OPT_UNSAFE) ||
            //         !(scan_dangerous_url(&node->as.link.url, 0))) {
            //     houdini_escape_href(html, node->as.link.url.data,
            //                         node->as.link.url.len);
            //   }
            //   cmark_strbuf_puts(html, "\" alt=\"");
            //   renderer->plain = node;
            // } else {
            //   if (node->as.link.title.len) {
            //     cmark_strbuf_puts(html, "\" title=\"");
            //     escape_html(html, node->as.link.title.data, node->as.link.title.len);
            //   }

            //   cmark_strbuf_puts(html, "\" />");
            // }

            if (entering)
            {
                std::string_view url{ (char*)node->as.link.url.data, (size_t)node->as.link.url.len };
                const auto urlHstring{ winrt::to_hstring(url) };
                Windows::Foundation::Uri uri{ data.baseUri, urlHstring };
                WUX::Controls::Image img{};
                WUX::Media::Imaging::BitmapImage bitmapImage;
                bitmapImage.UriSource(uri);
                img.Source(bitmapImage);
                WUX::Documents::InlineUIContainer imageBlock{};
                imageBlock.Child(img);
                data.currentParagraph().Inlines().Append(imageBlock);
            }
            else
            {
                data.EndSpan();
            }
            break;

        case CMARK_NODE_FOOTNOTE_DEFINITION:
            // if (entering) {
            //   if (renderer->footnote_ix == 0) {
            //     cmark_strbuf_puts(html, "<section class=\"footnotes\" data-footnotes>\n<ol>\n");
            //   }
            //   ++renderer->footnote_ix;

            //   cmark_strbuf_puts(html, "<li id=\"fn-");
            //   houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
            //   cmark_strbuf_puts(html, "\">\n");
            // } else {
            //   if (S_put_footnote_backref(renderer, html, node)) {
            //     cmark_strbuf_putc(html, '\n');
            //   }
            //   cmark_strbuf_puts(html, "</li>\n");
            // }
            break;

        case CMARK_NODE_FOOTNOTE_REFERENCE:
            // if (entering) {
            //   cmark_strbuf_puts(html, "<sup class=\"footnote-ref\"><a href=\"#fn-");
            //   houdini_escape_href(html, node->parent_footnote_def->as.literal.data, node->parent_footnote_def->as.literal.len);
            //   cmark_strbuf_puts(html, "\" id=\"fnref-");
            //   houdini_escape_href(html, node->parent_footnote_def->as.literal.data, node->parent_footnote_def->as.literal.len);

            //   if (node->footnote.ref_ix > 1) {
            //     char n[32];
            //     snprintf(n, sizeof(n), "%d", node->footnote.ref_ix);
            //     cmark_strbuf_puts(html, "-");
            //     cmark_strbuf_puts(html, n);
            //   }

            //   cmark_strbuf_puts(html, "\" data-footnote-ref>");
            //   houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
            //   cmark_strbuf_puts(html, "</a></sup>");
            // }
            break;

        default:
            assert(false);
            break;
        }
    }

    void parseMarkdown(const winrt::hstring& markdown, MyMarkdownData& data)
    {
        auto doc = cmark_parse_document(to_string(markdown).c_str(), markdown.size(), CMARK_OPT_DEFAULT);
        auto iter = cmark_iter_new(doc);
        cmark_event_type ev_type;
        cmark_node* curr;

        while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE)
        {
            curr = cmark_iter_get_node(iter);
            renderNode(curr, ev_type, data, 0);
            // bool entering = (ev_type == CMARK_EVENT_ENTER);
            //
            // // auto child = doc->first_child;
            // if (curr->type == CMARK_NODE_HEADING && entering)
            // {
            //     auto heading = curr->content;
            //     char* headingStr = (char*)malloc(heading.size + 1); // Allocate memory for the char*
            //     if (headingStr != NULL)
            //     {
            // cmark_strbuf_copy_cstr(headingStr, heading.size + 1, &heading);
            //     }
            //     auto level = curr->as.heading.level;
            //     // Paragraph paragraph = Paragraph();
            //     // Run run = Run();
            //     WUX::Controls::TextBlock tb{};
            //     tb.Text(winrt::to_hstring(headingStr));
            //     tb.FontSize((double)(14 + 6 * (6 - level)));
            //     // paragraph.Inlines().Append(run);
            //     RenderedMarkdown().Children().Append(tb);
            // }
            // else if (curr->type == CMARK_NODE_TEXT)
            // {
            //     WUX::Controls::TextBlock tb{};
            //     tb.Text(winrt::to_hstring(std::string_view{ (char*)curr->as.literal.data, (size_t)curr->as.literal.len }));
            //     RenderedMarkdown().Children().Append(tb);
            // }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////

    MarkdownPaneContent::MarkdownPaneContent() :
        MarkdownPaneContent(L"") {}

    MarkdownPaneContent::MarkdownPaneContent(const winrt::hstring& initialPath)
    {
        InitializeComponent();

        // auto bg = Resources().Lookup(winrt::box_value(L"PageBackground"));
        // auto brush = bg.try_as<WUX::Media::Brush>();
        // Background(brush);

        FilePathInput().Text(initialPath);
        _filePath = FilePathInput().Text();
        _loadFile();
    }

    INewContentArgs MarkdownPaneContent::GetNewTerminalArgs(BuildStartupKind /*kind*/) const
    {
        return BaseContentArgs(L"markdown");
    }

    void MarkdownPaneContent::_clearOldNotebook()
    {
        RenderedMarkdown().Children().Clear();
    }
    void MarkdownPaneContent::_loadFile()
    {
        // TODO! use our til::io file readers

        // Read _filePath, then parse as markdown.
        const wil::unique_handle file{ CreateFileW(_filePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr) };
        if (!file)
        {
            return;
        }

        char buffer[32 * 1024];
        DWORD read = 0;
        for (;;)
        {
            if (!ReadFile(file.get(), &buffer[0], sizeof(buffer), &read, nullptr))
            {
                break;
            }
            if (read < sizeof(buffer))
            {
                break;
            }
        }
        // BLINDLY TREATING TEXT AS utf-8 (I THINK)
        std::string markdownContents{ buffer, read };

        Editing(false);
        PropertyChanged.raise(*this, WUX::Data::PropertyChangedEventArgs{ L"Editing" });
        FileContents(winrt::to_hstring(markdownContents));
        PropertyChanged.raise(*this, WUX::Data::PropertyChangedEventArgs{ L"FileContents" });

        _renderFileContents();
    }
    void MarkdownPaneContent::_renderFileContents()
    {
        // Was the file a .md file?
        if (_filePath.ends_with(L".md"))
        {
            _loadMarkdown();
        }
        else
        {
            _loadText();
        }
    }
    void MarkdownPaneContent::_loadText()
    {
        auto block = WUX::Controls::TextBlock();
        block.IsTextSelectionEnabled(true);
        block.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
        block.Text(FileContents());

        RenderedMarkdown().Children().Append(block);
    }

    void MarkdownPaneContent::_loadMarkdown()
    {
        const auto& value{ FileContents() };
        MyMarkdownData data{};

        data.baseUri = _filePath;
        data.root.IsTextSelectionEnabled(true);
        data.root.TextWrapping(WUX::TextWrapping::WrapWholeWords);

        parseMarkdown(value, data);
        RenderedMarkdown().Children().Append(data.root);
    }

    void MarkdownPaneContent::_loadTapped(const Windows::Foundation::IInspectable&, const Windows::UI::Xaml::Input::TappedRoutedEventArgs&)
    {
        _filePath = FilePathInput().Text();
        // Does the file exist? if not, bail
        const wil::unique_handle file{ CreateFileW(_filePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr) };
        if (!file)
        {
            return;
        }

        // It does. Clear the old one
        _clearOldNotebook();
        _loadFile();
    }

    void MarkdownPaneContent::_editTapped(const Windows::Foundation::IInspectable&, const Windows::UI::Xaml::Input::TappedRoutedEventArgs&)
    {
        if (Editing())
        {
            _clearOldNotebook();
            _renderFileContents();

            EditIcon().Glyph(L"\xe932"); // Label

            _scrollViewer().Visibility(WUX::Visibility::Visible);
            _editor().Visibility(WUX::Visibility::Collapsed);

            Editing(false);
        }
        else
        {
            EditIcon().Glyph(L"\xe890"); // View

            _scrollViewer().Visibility(WUX::Visibility::Collapsed);
            _editor().Visibility(WUX::Visibility::Visible);

            Editing(true);
        }
        PropertyChanged.raise(*this, WUX::Data::PropertyChangedEventArgs{ L"Editing" });
    }

    void MarkdownPaneContent::_closeTapped(const Windows::Foundation::IInspectable&, const Windows::UI::Xaml::Input::TappedRoutedEventArgs&)
    {
        // // Clear the old one
        // _clearOldNotebook();
        // _loadFile();
        CloseRequested.raise(*this, nullptr);
    }

    void MarkdownPaneContent::_handleRunCommandRequest(const TerminalApp::CodeBlock& sender,
                                                       const TerminalApp::RequestRunCommandsArgs& request)
    {
        auto text = request.Commandlines();
        sender;
        text;

        if (const auto& strongControl{ _control.get() })
        {
            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, Model::SendInputArgs{ text + winrt::hstring{ L"\r" } } };

            // By using the last active control as the sender here, the
            // actiopn dispatch will send this to the active control,
            // thinking that it is the control that requested this event.
            DispatchActionRequested.raise(strongControl, actionAndArgs);
        }
    }

#pragma region IPaneContent

    winrt::Windows::UI::Xaml::FrameworkElement MarkdownPaneContent::GetRoot()
    {
        return *this;
    }

    void MarkdownPaneContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    winrt::hstring MarkdownPaneContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

#pragma endregion

    void MarkdownPaneContent::SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control)
    {
        _control = control;
    }
}
