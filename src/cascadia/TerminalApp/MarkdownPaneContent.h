// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MarkdownPaneContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct MarkdownPaneContent : MarkdownPaneContentT<MarkdownPaneContent>
    {
    public:
        MarkdownPaneContent();
        MarkdownPaneContent(const winrt::hstring& filePath);

        void SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control);

#pragma region IPaneContent
        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings&){};

        winrt::Windows::Foundation::Size MinSize() { return { 1, 1 }; };
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic) { reason; };
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool /*asContent*/) const { return nullptr; };

        // TODO! lots of strings here and in XAML that need RS_-ifying
        winrt::hstring Title() { return _filePath; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush() { return Background(); }

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

        til::typed_event<winrt::Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command> DispatchCommandRequested;
#pragma endregion

        void _handleRunCommandRequest(const TerminalApp::CodeBlock& sender,
                                      const TerminalApp::RequestRunCommandsArgs& control);

    private:
        friend struct MarkdownPaneContentT<MarkdownPaneContent>; // for Xaml to bind events

        // void _createNotebook();
        void _clearOldNotebook();
        void _loadMarkdown();

        // winrt::Microsoft::Terminal::Control::Notebook _notebook{ nullptr };
        winrt::hstring _filePath{};

        void _loadTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
        void _reloadTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);

        // void _newBlockHandler(const winrt::Microsoft::Terminal::Control::Notebook& sender,
        //                       const winrt::Microsoft::Terminal::Control::NotebookBlock& control);
        // void _addControl(const winrt::Microsoft::Terminal::Control::TermControl& control);
        // void _scrollToElement(const Windows::UI::Xaml::UIElement& element,
        //                       bool isVerticalScrolling = true,
        //                       bool smoothScrolling = true);

        // winrt::fire_and_forget _stupid(Windows::UI::Xaml::UIElement elem);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(MarkdownPaneContent);
}
