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
    // struct MyMarkdownData
    // {
    //     WUX::Controls::StackPanel root{};
    //     implementation::MarkdownPaneContent* page{ nullptr };
    //     WUX::Controls::TextBlock current{ nullptr };
    //     WUX::Documents::Run currentRun{ nullptr };
    //     TerminalApp::CodeBlock currentCodeBlock{ nullptr };
    // };
    // WUX::Controls::TextBlock makeDefaultTextBlock()
    // {
    //     WUX::Controls::TextBlock b{};
    //     b.IsTextSelectionEnabled(true);
    //     b.TextWrapping(WUX::TextWrapping::WrapWholeWords);
    //     return b;
    // }
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
        // MyMarkdownData data;
        // data.page = this;

        //const auto parseResult = 0; // parseMarkdown(FileContents(), data);

        //if (0 == parseResult)
        //{
        //    RenderedMarkdown().Children().Append(data.root);
        //}

        const auto& value{ FileContents() };

        auto doc = cmark_parse_document(to_string(value).c_str(), value.size(), CMARK_OPT_DEFAULT);
        auto iter = cmark_iter_new(doc);
        cmark_event_type ev_type;
        cmark_node* curr;

        while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE)
        {
            bool entering = (ev_type == CMARK_EVENT_ENTER);
            curr = cmark_iter_get_node(iter);
            // auto child = doc->first_child;
            if (curr->type == CMARK_NODE_HEADING && entering)
            {
                auto heading = curr->content;
                char* headingStr = (char*)malloc(heading.size + 1); // Allocate memory for the char*
                if (headingStr != NULL)
                {
                    cmark_strbuf_copy_cstr(headingStr, heading.size + 1, &heading);
                }
                auto level = curr->as.heading.level;
                // Paragraph paragraph = Paragraph();
                // Run run = Run();
                WUX::Controls::TextBlock tb{};
                tb.Text(winrt::to_hstring(headingStr));
                tb.FontSize((double)(14 + 6 * (6 - level)));
                // paragraph.Inlines().Append(run);
                RenderedMarkdown().Children().Append(tb);
            }
            else if (curr->type == CMARK_NODE_TEXT)
            {
                WUX::Controls::TextBlock tb{};
                tb.Text(winrt::to_hstring(std::string_view{ (char*)curr->as.literal.data, (size_t)curr->as.literal.len } ));
                RenderedMarkdown().Children().Append(tb);
            }
        }
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
