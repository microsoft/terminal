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

    struct MyMarkdownData
    {
        WUX::Controls::RichTextBlock root{};
        implementation::MarkdownPaneContent* page{ nullptr };
        winrt::hstring baseUri{ L"" };
        WUX::Controls::TextBlock current{ nullptr };
        WUX::Documents::Run _currentRun{ nullptr };
        WUX::Documents::Span _currentSpan{ nullptr };
        WUX::Documents::Paragraph lastParagraph{ nullptr };
        TerminalApp::CodeBlock currentCodeBlock{ nullptr };
        WUX::Controls::Image currentImage{ nullptr };
        int indent = 0;
        int blockQuoteDepth = 0;

        WUX::Documents::Paragraph currentParagraph()
        {
            if (lastParagraph == nullptr)
            {
                EndRun(); // sanity check
                lastParagraph = WUX::Documents::Paragraph();
                if (indent > 0)
                {
                    if (indent - blockQuoteDepth > 0)
                    {
                        lastParagraph.TextIndent(-12);
                    }
                    lastParagraph.Margin(WUX::ThicknessHelper::FromLengths(18 * indent, 0, 0, 0));
                }
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

    void renderNode(cmark_node* node, cmark_event_type ev_type, MyMarkdownData& data, int /*options*/)
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
                data.EndParagraph();
                data.indent++;
                data.blockQuoteDepth++;
            }
            else
            {
                data.EndParagraph();
                data.indent = std::max(0, data.indent - 1);
                data.blockQuoteDepth = std::max(0, data.blockQuoteDepth - 1);
            }

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

            // cmark_list_type list_type = node->as.list.list_type;
            // int start = node->as.list.start;
            if (entering)
            {
                data.EndParagraph();
                data.indent++;
                // data.lastParagraph = WUX::Documents::Paragraph();
                // data.lastParagraph.TextIndent(24 * data.indent);
                // root.Blocks().Append(lastParagraph);
            }
            else
            {
                data.EndParagraph();
                data.indent = std::max(0, data.indent - 1);
            }
            break;
        }

        case CMARK_NODE_ITEM:

            // A list item, either for a ordered list or an unordered one.

            if (entering)
            {
                data.EndParagraph();
                data.currentParagraph();
                data.newRun().Text(winrt::hstring{ bullets[std::clamp(data.indent - data.blockQuoteDepth - 1, 0, 2)] });
            }
            break;

        case CMARK_NODE_HEADING:
            data.EndParagraph();

            // At the start of a header, change the font size to match the new
            // level of header we're at. The text will come later, in a
            // CMARK_NODE_TEXT
            if (entering)
            {
                const auto level = node->as.heading.level;
                data.currentParagraph().FontSize(std::max(16u, 36u - ((level - 1) * 6u)));
            }
            break;

        case CMARK_NODE_CODE_BLOCK:
        {
            data.EndParagraph();

            std::string_view code{ (char*)node->as.code.literal.data, (size_t)node->as.code.literal.len - 1 };
            const auto codeHstring{ winrt::to_hstring(code) };

            auto codeBlock = winrt::make<implementation::CodeBlock>(codeHstring);
            codeBlock.RequestRunCommands({ data.page, &MarkdownPaneContent::_handleRunCommandRequest });
            WUX::Documents::InlineUIContainer codeContainer{};
            codeContainer.Child(codeBlock);
            data.currentParagraph().Inlines().Append(codeContainer);

            data.EndParagraph();
            data.currentParagraph();
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
                data.EndParagraph();
            }

            // Start a new paragraph if we don't have one
            data.currentParagraph();
            break;
        }
        case CMARK_NODE_TEXT:
        {
            const auto text{ winrt::to_hstring(textFromLiteral(node)) };

            if (data.currentImage)
            {
                // The tooltip for an image comes in as a CMARK_NODE_TEXT, so set that here.
                WUX::Controls::ToolTipService::SetToolTip(data.currentImage, box_value(text));
            }
            else
            {
                // Otherwise, just add the text to the current paragraph
                data.newRun().Text(text);
            }
        }

        break;

        case CMARK_NODE_LINEBREAK:
            data.EndSpan();
            data.currentParagraph().Inlines().Append(WUX::Documents::LineBreak());
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

            data.newRun().Text(L" ");
            break;

        case CMARK_NODE_CODE:
        {
            const auto text{ winrt::to_hstring(textFromLiteral(node)) };
            const auto& codeRun{ data.newRun() };

            codeRun.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
            // A Span can't have a border or a background, so we can't give
            // it the whole treatment that a <code> span gets in HTML.
            codeRun.Text(text);

            data.newRun().FontFamily(data.root.FontFamily());
        }
        break;

        case CMARK_NODE_HTML_INLINE:
            // Same as above - no raw HTML support here.
            break;

        case CMARK_NODE_CUSTOM_INLINE:
            // Same as above - not even entirely sure what this is.
            break;

        case CMARK_NODE_STRONG:
            data.newRun().FontWeight(entering ?
                                         Windows::UI::Text::FontWeights::Bold() :
                                         Windows::UI::Text::FontWeights::Normal());
            break;

        case CMARK_NODE_EMPH:
            data.newRun().FontStyle(entering ?
                                        Windows::UI::Text::FontStyle::Italic :
                                        Windows::UI::Text::FontStyle::Normal);
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
                const auto urlHstring{ winrt::to_hstring(url) };
                // TODO! add tooltip that does the unescaped URL thing that we do for termcontrol
                WUX::Documents::Hyperlink a{};
                Windows::Foundation::Uri uri{ data.baseUri, urlHstring };
                a.NavigateUri(uri);
                data.currentParagraph().Inlines().Append(a);
                data._currentSpan = a;

                // Similar to the header element, the actual text of the link
                // will later come through as a CMARK_NODE_TEXT
            }
            else
            {
                data.EndSpan();
            }
            break;

        case CMARK_NODE_IMAGE:
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
                data.currentImage = img;
            }
            else
            {
                data.EndSpan();
                data.currentImage = nullptr;
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
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    MarkdownPaneContent::MarkdownPaneContent() :
        MarkdownPaneContent(L"") {}

    MarkdownPaneContent::MarkdownPaneContent(const winrt::hstring& initialPath)
    {
        InitializeComponent();

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

        data.page = this;
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
            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, Model::SendInputArgs{ text } };

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
