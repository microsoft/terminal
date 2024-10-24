// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "SnippetsPaneContent.g.h"
#include "FilteredTask.g.h"
#include "BasicPaneEvents.h"
#include "FilteredCommand.h"
#include "ActionPaletteItem.h"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    struct SnippetsPaneContent : SnippetsPaneContentT<SnippetsPaneContent>, BasicPaneEvents
    {
        SnippetsPaneContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        safe_void_coroutine UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

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

    struct FilteredTask : FilteredTaskT<FilteredTask>
    {
        FilteredTask() = default;

        FilteredTask(const winrt::Microsoft::Terminal::Settings::Model::Command& command)
        {
            _filteredCommand = winrt::make_self<implementation::FilteredCommand>(winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(command, winrt::hstring{}));
            _command = command;

            // The Children() method must always return a non-null vector
            _children = winrt::single_threaded_observable_vector<TerminalApp::FilteredTask>();
            if (_command.HasNestedCommands())
            {
                for (const auto& [_, child] : _command.NestedCommands())
                {
                    auto vm{ winrt::make<FilteredTask>(child) };
                    _children.Append(vm);
                }
            }
        }

        void UpdateFilter(const winrt::hstring& filter)
        {
            _filteredCommand->UpdateFilter(filter);
            for (const auto& c : _children)
            {
                auto impl = winrt::get_self<implementation::FilteredTask>(c);
                impl->UpdateFilter(filter);
            }

            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Visibility" });
        }

        winrt::hstring Input()
        {
            if (const auto& actionItem{ _filteredCommand->Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
            {
                if (const auto& command{ actionItem.Command() })
                {
                    if (const auto& sendInput{ command.ActionAndArgs().Args().try_as<winrt::Microsoft::Terminal::Settings::Model::SendInputArgs>() })
                    {
                        return winrt::hstring{ til::visualize_nonspace_control_codes(std::wstring{ sendInput.Input() }) };
                    }
                }
            }
            return winrt::hstring{};
        };

        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> Children() { return _children; }
        bool HasChildren() { return _children.Size() > 0; }
        winrt::Microsoft::Terminal::Settings::Model::Command Command() { return _command; }
        winrt::TerminalApp::FilteredCommand FilteredCommand() { return *_filteredCommand; }

        int32_t Row() { return HasChildren() ? 2 : 1; } // See the BODGY comment in the .XAML for explanation

        // Used to control if this item is visible in the TreeView. Turns out,
        // TreeView is in fact sane enough to remove items entirely if they're
        // Collapsed.
        winrt::Windows::UI::Xaml::Visibility Visibility()
        {
            // Is there no filter, or do we match it?
            if (_filteredCommand->Filter().empty() || _filteredCommand->Weight() > 0)
            {
                return winrt::Windows::UI::Xaml::Visibility::Visible;
            }
            // If we don't match, maybe one of our children does
            auto totalWeight = _filteredCommand->Weight();
            for (const auto& c : _children)
            {
                auto impl = winrt::get_self<implementation::FilteredTask>(c);
                totalWeight += impl->_filteredCommand->Weight();
            }

            return totalWeight > 0 ? winrt::Windows::UI::Xaml::Visibility::Visible : winrt::Windows::UI::Xaml::Visibility::Collapsed;
        };

        til::property_changed_event PropertyChanged;

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command{ nullptr };
        winrt::com_ptr<winrt::TerminalApp::implementation::FilteredCommand> _filteredCommand{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _children{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SnippetsPaneContent);
}
