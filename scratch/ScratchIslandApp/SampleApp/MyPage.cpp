// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "MySettings.h"
#include "CodeBlock.h"
#define MD4C_USE_UTF16
#include "..\..\..\oss\md4c\md4c.h"

using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::SampleApp::implementation
{
    int md_parser_enter_block(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* /*userdata*/) { return 0; }
    int md_parser_leave_block(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* /*userdata*/) { return 0; }
    int md_parser_enter_span(MD_SPANTYPE /*type*/, void* /*detail*/, void* /*userdata*/) { return 0; }
    int md_parser_leave_span(MD_SPANTYPE /*type*/, void* /*detail*/, void* /*userdata*/) { return 0; }
    int md_parser_text(MD_TEXTTYPE /*type*/, const MD_CHAR* /*text*/, MD_SIZE /*size*/, void* /*userdata*/) { return 0; }

    void parseMarkdown(const std::wstring& markdown)
    {
        MD_PARSER parser{
            .abi_version = 0,
            .flags = 0,
            .enter_block = &md_parser_enter_block,
            .leave_block = &md_parser_leave_block,
            .enter_span = &md_parser_enter_span,
            .leave_span = &md_parser_leave_span,
            .text = &md_parser_text,
        };

        const auto result = md_parse(
            markdown.c_str(),
            (unsigned)markdown.size(),
            &parser,
            nullptr // user data
        );

        result;
    }

    // void markdownToXaml(const MD_CHAR* input, size_t inputSize, std::vector<std::wstring>& output)
    // {
    //     MD_STRUCT* doc = md4c_parse(input, inputSize, 0);
    //     if (!doc)
    //     {
    //         // cerr << "Failed to parse Markdown" << endl;
    //         return;
    //     }

    //     std::stack<std::wstring> tags; // Stack to keep track of open tags
    //     tags.push(L""); // Push empty tag to handle root elements

    //     for (MD_BLOCK* block = doc->first_child; block != nullptr; block = block->next)
    //     {
    //         switch (block->type)
    //         {
    //         case MD_BLOCK_DOC:
    //             break; // Ignore document node
    //         case MD_BLOCK_HR:
    //             output.push_back(L"<Separator/>");
    //             break;
    //         case MD_BLOCK_H:
    //         {
    //             int level = block->header.level;
    //             std::wstring headerTag = L"<TextBlock Text=\"";
    //             headerTag += std::wstring(level, L'#') + L" " + std::wstring(block->string, block->len) + L"\"/>";
    //             output.push_back(headerTag);
    //             break;
    //         }
    //         case MD_BLOCK_P:
    //             output.push_back(L"<TextBlock Text=\"" + std::wstring(block->string, block->len) + L"\"/>");
    //             break;
    //         case MD_BLOCK_CODE:
    //             output.push_back(L"<TextBlock Text=\"" + std::wstring(block->string, block->len) + L"\"/>");
    //             break;
    //         case MD_BLOCK_QUOTE:
    //             output.push_back(L"<TextBlock Text=\"" + std::wstring(block->string, block->len) + L"\" FontStyle=\"Italic\"/>");
    //             break;
    //         case MD_BLOCK_UL:
    //         case MD_BLOCK_OL:
    //             tags.push(L"<StackPanel>");
    //             break;
    //         case MD_BLOCK_LI:
    //             output.push_back(tags.top() + L"<TextBlock Text=\"" + std::wstring(block->string, block->len) + L"\"/>");
    //             break;
    //         case MD_BLOCK_HTML:
    //             // Handle HTML blocks if necessary
    //             break;
    //         }
    //     }

    //     while (!tags.empty())
    //     {
    //         output.push_back(tags.top() + L"</StackPanel>");
    //         tags.pop();
    //     }

    //     md4c_free(doc); // Free parsed Markdown AST
    // }

    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create()
    {
        // {
        //     // Example Markdown input
        //     const char* markdownInput = "## Hello, *world*!\nThis is a **sample** Markdown text.\n"
        //                                 "> This is a blockquote.\n"
        //                                 "- List item 1\n"
        //                                 "- List item 2\n"
        //                                 "1. Numbered item 1\n"
        //                                 "2. Numbered item 2\n";

        //     // Convert Markdown to XAML
        //     std::vector<wstring> xamlOutput;
        //     markdownToXaml(markdownInput, strlen(markdownInput), xamlOutput);
        //     xamlOutput;
        // }

        {
            std::wstring markdown{ LR"(
# readme

This is my cool project

## build

To build the thing, run the following command:

```cmd
build the_thing
```

## test

```cmd
pwsh -c gci ~
ping 8.8.8.8
```

That'll run the tests
)" };
            parseMarkdown(markdown);
        }

        // First things first, make a dummy code block
        const auto createCodeBlock = [=](const auto& command) {
            auto codeBlock = winrt::make<implementation::CodeBlock>(command);

            codeBlock.Margin(WUX::ThicknessHelper::FromLengths(8, 8, 8, 8));

            codeBlock.RequestRunCommands({ this, &MyPage::_handleRunCommandRequest });

            OutOfProcContent().Children().Append(codeBlock);
        };

        createCodeBlock(L"ping 8.8.8.8");
        createCodeBlock(L"echo This has been a test of the new code block objects");
        createCodeBlock(L"set FOO=%FOO%+1 & echo FOO set to %FOO%");
        createCodeBlock(L"cd /d %~%");
        createCodeBlock(L"cd /d z:\\dev\\public\\OpenConsole");
        createCodeBlock(L"pwsh -c gci");
        createCodeBlock(L"git status");

        _createOutOfProcContent();
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring MyPage::Title()
    {
        return { L"Sample Application" };
    }

    void MyPage::_createOutOfProcContent()
    {
        auto settings = winrt::make_self<implementation::MySettings>();

        settings->DefaultBackground(til::color{ 0x25, 0x25, 0x25 });
        settings->AutoMarkPrompts(true);
        auto envMap = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        envMap.Insert(L"PROMPT", L"$e]133;D$e\\$e]133;A$e\\$e]9;9;$P$e\\$P$G$e]133;B$e\\");

        auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This a notebook connection.",
                                                                                      winrt::hstring{},
                                                                                      L"",
                                                                                      false,
                                                                                      L"",
                                                                                      envMap.GetView(),
                                                                                      32,
                                                                                      80,
                                                                                      winrt::guid(),
                                                                                      winrt::guid()) };

        // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
        winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, connectionSettings };

        TerminalConnection::ITerminalConnection conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectInfo) };

        _notebook = Control::Notebook(*settings, *settings, conn);
        // _notebook.Initialize();
        // _notebook.NewBlock({ get_weak(), &MyPage::_newBlockHandler });

        // _addControl(_notebook.ActiveBlock().Control());
    }

    void MyPage::_newBlockHandler(const Control::Notebook& /*sender*/,
                                  const Control::NotebookBlock& block)
    {
        _addControl(block.Control());
    }

    void MyPage::_handleRunCommandRequest(const SampleApp::CodeBlock& sender,
                                          const SampleApp::RequestRunCommandsArgs& request)
    {
        // _addControl(block.Control());
        auto text = request.Commandlines();
        auto targetControl = _notebook.ActiveBlock().Control();

        sender.OutputBlock(_notebook.ActiveBlock());

        targetControl.Height(256);
        targetControl.VerticalAlignment(WUX::VerticalAlignment::Top);
        targetControl.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        targetControl.Initialized([text](const auto& sender, auto&&) {
            sender.SendInput(text);
            sender.SendInput(L"\r");
        });
        // targetControl.SendInput(text);
        // targetControl.SendInput(L"\r");
        // _stupid(targetControl);
    }

    void MyPage::_scrollToElement(const WUX::UIElement& element,
                                  bool isVerticalScrolling,
                                  bool smoothScrolling)
    {
        const auto scrollViewer = _scrollViewer();

        const auto origin = winrt::Windows::Foundation::Point{ 0, 0 };

        const auto transform_scrollContent = element.TransformToVisual(scrollViewer.Content().try_as<WUX::UIElement>());
        const auto position_scrollContent = transform_scrollContent.TransformPoint(origin);

        if (isVerticalScrolling)
        {
            scrollViewer.ChangeView(nullptr, position_scrollContent.Y, nullptr, !smoothScrolling);
        }
        else
        {
            scrollViewer.ChangeView(position_scrollContent.X, nullptr, nullptr, !smoothScrolling);
        }
    }

    void MyPage::_addControl(const Control::TermControl& control)
    {
        control.Height(256);
        control.VerticalAlignment(WUX::VerticalAlignment::Top);
        control.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);

        WUX::Controls::Grid wrapper{};
        wrapper.VerticalAlignment(WUX::VerticalAlignment::Top);
        wrapper.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        wrapper.CornerRadius(WUX::CornerRadiusHelper::FromRadii(6, 6, 6, 6));
        wrapper.Margin(WUX::ThicknessHelper::FromLengths(0, 5, 0, 7));
        wrapper.Children().Append(control);

        OutOfProcContent().Children().Append(wrapper);

        control.Focus(WUX::FocusState::Programmatic);

        // Incredibly dumb: move off UI thread, then back on, then scroll to the
        // new control.
        _stupid(wrapper);
    }

    winrt::fire_and_forget MyPage::_stupid(WUX::UIElement elem)
    {
        co_await winrt::resume_after(2ms); // no, resume_background is not enough to make this work.
        co_await winrt::resume_foreground(this->Dispatcher(), winrt::Windows::UI::Core::CoreDispatcherPriority::Low);
        _scrollToElement(elem);
    }
}
