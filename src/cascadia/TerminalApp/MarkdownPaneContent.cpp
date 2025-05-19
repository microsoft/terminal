// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MarkdownPaneContent.h"
#include <LibraryResources.h>
#include "MarkdownPaneContent.g.cpp"
#include <til/io.h>

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
        return BaseContentArgs(L"x-markdown");
    }

    void MarkdownPaneContent::_clearOldNotebook()
    {
        RenderedMarkdown().Children().Clear();
    }
    void MarkdownPaneContent::_loadFile()
    {
        if (_filePath.empty())
        {
            return;
        }

        // Our title is the path of our MD file
        TitleChanged.raise(*this, nullptr);

        const std::filesystem::path filePath{ std::wstring_view{ _filePath } };
        const auto markdownContents{ til::io::read_file_as_utf8_string_if_exists(filePath) };

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
        block.ContextFlyout(winrt::Microsoft::Terminal::UI::TextMenuFlyout{});
        block.IsTextSelectionEnabled(true);
        block.FontFamily(WUX::Media::FontFamily{ L"Cascadia Code" });
        block.Text(FileContents());

        RenderedMarkdown().Children().Append(block);
    }

    void MarkdownPaneContent::_loadMarkdown()
    {
        auto rootTextBlock{ Microsoft::Terminal::UI::Markdown::Builder::Convert(FileContents(), _filePath) };

        // By default, the markdown pane doesn't have play buttons next to the
        // blocks. But to demonstrate how that's possible:
        for (const auto& b : rootTextBlock.Blocks())
        {
            if (const auto& p{ b.try_as<WUX::Documents::Paragraph>() })
            {
                for (const auto& line : p.Inlines())
                {
                    if (const auto& otherContent{ line.try_as<WUX::Documents::InlineUIContainer>() })
                    {
                        if (const auto& codeBlock{ otherContent.Child().try_as<Microsoft::Terminal::UI::Markdown::CodeBlock>() })
                        {
                            codeBlock.PlayButtonVisibility(WUX::Visibility::Visible);
                            codeBlock.RequestRunCommands({ this, &MarkdownPaneContent::_handleRunCommandRequest });
                        }
                    }
                }
            }
        }

        RenderedMarkdown().Children().Append(rootTextBlock);
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

    void MarkdownPaneContent::_handleRunCommandRequest(const Microsoft::Terminal::UI::Markdown::CodeBlock& /*sender*/,
                                                       const Microsoft::Terminal::UI::Markdown::RequestRunCommandsArgs& request)
    {
        auto text = request.Commandlines();

        if (const auto& strongControl{ _control.get() })
        {
            Model::ActionAndArgs actionAndArgs{ ShortcutAction::SendInput, Model::SendInputArgs{ text } };

            // By using the last active control as the sender here, the
            // action dispatch will send this to the active control,
            // thinking that it is the control that requested this event.
            DispatchActionRequested.raise(strongControl, actionAndArgs);
            strongControl.Focus(winrt::WUX::FocusState::Programmatic);
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
