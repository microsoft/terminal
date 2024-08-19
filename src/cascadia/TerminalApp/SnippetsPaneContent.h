// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "SnippetsPaneContent.g.h"
#include "FilteredTask.h"
#include "BasicPaneEvents.h"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    struct SnippetsPaneContent : SnippetsPaneContentT<SnippetsPaneContent>, BasicPaneEvents
    {
        SnippetsPaneContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        winrt::fire_and_forget UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinimumSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title() { return RS_(L"SnippetPaneTitle/Text"); }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        void SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control);
        bool HasSnippets() const;

        // See BasicPaneEvents for most generic event definitions

        til::property_changed_event PropertyChanged;

    private:
        friend struct SnippetsPaneContentT<SnippetsPaneContent>; // for Xaml to bind events

        winrt::weak_ref<Microsoft::Terminal::Control::TermControl> _control{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _allTasks{ nullptr };

        void _runCommandButtonClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);
        void _closePaneClick(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);
        void _filterTextChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        void _updateFilteredCommands();
        void _treeItemInvokedHandler(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::TreeViewItemInvokedEventArgs& e);
        void _treeItemKeyUpHandler(const IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _runCommand(const Microsoft::Terminal::Settings::Model::Command& command);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SnippetsPaneContent);
}
