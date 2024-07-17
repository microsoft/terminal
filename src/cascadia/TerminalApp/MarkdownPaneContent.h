// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MarkdownPaneContent.g.h"
#include "BasicPaneEvents.h"

namespace winrt::TerminalApp::implementation
{
    struct MarkdownPaneContent : MarkdownPaneContentT<MarkdownPaneContent>, BasicPaneEvents
    {
    public:
        MarkdownPaneContent();
        MarkdownPaneContent(const winrt::hstring& filePath);

        til::property<bool> Editing{ false };
        til::property<winrt::hstring> FileContents{ L"" };

        void SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control);

        // TODO! this should just be til::property_changed_event but I don't have that commit here
        til::event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> PropertyChanged;

#pragma region IPaneContent
        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings&){};

        winrt::Windows::Foundation::Size MinimumSize() { return { 1, 1 }; };
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic) { reason; };
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title() { return _filePath; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush() { return Background(); }

        // See BasicPaneEvents for most generic event definitions

#pragma endregion

        til::typed_event<winrt::Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::ActionAndArgs> DispatchActionRequested;

        void _handleRunCommandRequest(const Microsoft::Terminal::UI::Markdown::CodeBlock& sender,
                                      const Microsoft::Terminal::UI::Markdown::RequestRunCommandsArgs& control);

    private:
        friend struct MarkdownPaneContentT<MarkdownPaneContent>; // for Xaml to bind events

        winrt::hstring _filePath{};

        winrt::weak_ref<Microsoft::Terminal::Control::TermControl> _control{ nullptr };

        void _clearOldNotebook();
        void _loadFile();
        void _renderFileContents();
        void _loadText();
        void _loadMarkdown();

        void _loadTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
        void _editTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
        void _closeTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(MarkdownPaneContent);
}
