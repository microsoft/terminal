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

    struct MyMarkdownData
    {
        WUX::Controls::StackPanel root{};
        implementation::MyPage* page{ nullptr };
        // std::vector<WUX::Documents::Run> currentRuns{};
        WUX::Controls::TextBlock current{ nullptr };
        WUX::Documents::Run currentRun{ nullptr };
        // winrt::com_ptr<implementation::CodeBlock> currentCodeBlock{ nullptr };
        SampleApp::CodeBlock currentCodeBlock{ nullptr };
    };

    int md_parser_enter_block(MD_BLOCKTYPE type, void* detail, void* userdata)
    {
        MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
        data;
        switch (type)
        {
        case MD_BLOCK_UL:
        {
            break;
        }
        case MD_BLOCK_H:
        {
            MD_BLOCK_H_DETAIL* headerDetail = reinterpret_cast<MD_BLOCK_H_DETAIL*>(detail);
            data->current = WUX::Controls::TextBlock{};
            const auto fontSize = std::max(16u, 36u - ((headerDetail->level - 1) * 6u));
            data->current.FontSize(fontSize);
            data->current.FontWeight(Windows::UI::Text::FontWeights::Bold());
            WUX::Documents::Run run{};
            // run.Text(winrtL'#');

            // Immediately add the header block
            data->root.Children().Append(data->current);

            if (headerDetail->level == 1)
            {
                // <Border Height="1" BorderThickness="1" BorderBrush="Red" HorizontalAlignment="Stretch"></Border>
                WUX::Controls::Border b;
                b.Height(1);
                b.BorderThickness(WUX::ThicknessHelper::FromLengths(1, 1, 1, 1));
                b.BorderBrush(WUX::Media::SolidColorBrush(Windows::UI::Colors::Gray()));
                b.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
                data->root.Children().Append(b);
            }
            break;
        }
        case MD_BLOCK_CODE:
        {
            MD_BLOCK_CODE_DETAIL* codeDetail = reinterpret_cast<MD_BLOCK_CODE_DETAIL*>(detail);
            codeDetail;

            data->currentCodeBlock = winrt::make<implementation::CodeBlock>(L"");
            data->currentCodeBlock.Margin(WUX::ThicknessHelper::FromLengths(8, 8, 8, 8));
            data->currentCodeBlock.RequestRunCommands({ data->page, &MyPage::_handleRunCommandRequest });

            data->root.Children().Append(data->currentCodeBlock);
        }
        default:
        {
            break;
        }
        }
        return 0;
    }
    int md_parser_leave_block(MD_BLOCKTYPE type, void* /*detail*/, void* userdata)
    {
        MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
        data;
        switch (type)
        {
        case MD_BLOCK_UL:
        {
            break;
        }
        case MD_BLOCK_H:
        {
            // data->root.Children().Append(data->current);
            data->current = nullptr;
            break;
        }
        case MD_BLOCK_CODE:
        {
            // data->root.Children().Append(data->current);
            data->current = nullptr;
            break;
        }
        default:
        {
            break;
        }
        }
        return 0;
    }
    int md_parser_enter_span(MD_SPANTYPE type, void* /*detail*/, void* userdata)
    {
        MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
        data;

        if (data->current == nullptr)
        {
            data->current = WUX::Controls::TextBlock();
            data->root.Children().Append(data->current);
        }
        if (data->currentRun == nullptr)
        {
            data->currentRun = WUX::Documents::Run();
        }
        auto currentRun = data->currentRun;
        switch (type)
        {
        case MD_SPAN_STRONG:
        {
            currentRun.FontWeight(Windows::UI::Text::FontWeights::Bold());
            break;
        }
        case MD_SPAN_EM:
        {
            currentRun.FontStyle(Windows::UI::Text::FontStyle::Italic);
            break;
        }
        case MD_SPAN_CODE:
        {
            currentRun.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
            break;
        }
        default:
        {
            break;
        }
        }
        return 0;
    }
    int md_parser_leave_span(MD_SPANTYPE type, void* /*detail*/, void* userdata)
    {
        MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
        switch (type)
        {
        case MD_SPAN_EM:
        case MD_SPAN_STRONG:
        // {
        //     break;
        // }
        case MD_SPAN_CODE:
        {
            if (const auto& currentRun{ data->currentRun })
            {
                // data->current.Inlines().Append(currentRun);
                // data->currentRun = nullptr;
            }
            break;
        }
        default:
        {
            break;
        }
        }
        return 0;
    }
    int md_parser_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
    {
        MyMarkdownData* data = reinterpret_cast<MyMarkdownData*>(userdata);
        winrt::hstring str{ text, size };
        switch (type)
        {
        case MD_TEXT_BR:
        case MD_TEXT_SOFTBR:
        {
            if (const auto& curr{ data->current })
            {
                data->root.Children().Append(curr);
                data->current = WUX::Controls::TextBlock();
            }

            break;
        }
        case MD_TEXT_CODE:
        {
            if (str == L"\n")
            {
                break;
            }
            if (const auto& codeBlock{ data->currentCodeBlock })
            {
                // code in a fenced block
                auto currentText = codeBlock.Commandlines();
                auto newText = currentText.empty() ? str :
                                                     currentText + winrt::hstring{ L"\r\n" } + str;
                codeBlock.Commandlines(newText);
                break;
            }
            else
            {
                // just normal `code` inline
                // data->currentRun.Text(str);
                [[fallthrough]];
            }
        }
        case MD_TEXT_NORMAL:
        default:
        {
            auto run = data->currentRun ? data->currentRun : WUX::Documents::Run{};
            run.Text(str);
            if (data->current)
            {
                data->current.Inlines().Append(run);
            }
            else
            {
                WUX::Controls::TextBlock block{};
                block.Inlines().Append(run);
                data->root.Children().Append(block);
                data->current = block;
            }
            // data->root.Children().Append(block);

            data->currentRun = nullptr;
            break;
        }
        }
        return 0;
    }

    int parseMarkdown(const std::wstring& markdown, MyMarkdownData& data)
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
            &data // user data
        );

        return result;
    }

    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create()
    {
        {
            std::wstring markdown{ LR"(
# Readme

This is my cool project. It's got lots of commands.

## Useful directories

Click these to `cd` the notebook to relevant locations.

```
cd /d %~%
```

```
cd /d z:\dev\public\OpenConsole
```

## build

### Setup

Dependencies!

```
winget search "I most certainly don't exist"
```

### Actual build

To build the thing, run the following command:

```cmd
build the_thing
```

## test

```cmd
pwsh -c gci
ping 8.8.8.8
```

That _should_ run the tests

## Other helpful commmands

```
git status
```
```
git --no-pager diff dev/migrie/fhl/2024-spring-merge-base --stat -- . ":!oss/md4c"
```
```
set FOO=%FOO%+1 & echo FOO set to %FOO%
```
```
echo This has been a test of the new code block objects
```


)" };
            MyMarkdownData data;
            data.page = this;

            if (0 == parseMarkdown(markdown, data))
            {
                OutOfProcContent().Children().Append(data.root);
            }
        }

        // // First things first, make a dummy code block
        // const auto createCodeBlock = [=](const auto& command) {
        //     auto codeBlock = winrt::make<implementation::CodeBlock>(command);

        //     codeBlock.Margin(WUX::ThicknessHelper::FromLengths(8, 8, 8, 8));

        //     codeBlock.RequestRunCommands({ this, &MyPage::_handleRunCommandRequest });

        //     OutOfProcContent().Children().Append(codeBlock);
        // };

        // createCodeBlock(L"ping 8.8.8.8");
        // createCodeBlock(L"echo This has been a test of the new code block objects");
        // createCodeBlock(L"set FOO=%FOO%+1 & echo FOO set to %FOO%");
        // createCodeBlock(L"cd /d %~%");
        // createCodeBlock(L"cd /d z:\\dev\\public\\OpenConsole");
        // createCodeBlock(L"pwsh -c gci");
        // createCodeBlock(L"git status");

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
