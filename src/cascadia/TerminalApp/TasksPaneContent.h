// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TasksPaneContent.g.h"
#include "FilteredTask.g.h"
#include "FilteredCommand.h"
#include "ActionPaletteItem.h"

namespace winrt::TerminalApp::implementation
{
    struct TasksPaneContent : TasksPaneContentT<TasksPaneContent>
    {
        TasksPaneContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinimumSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        // TODO! lots of strings here and in XAML that need RS_-ifying
        winrt::hstring Title() { return L"Tasks"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        void SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control);

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

        til::typed_event<winrt::Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command> DispatchCommandRequested;

    private:
        friend struct TasksPaneContentT<TasksPaneContent>; // for Xaml to bind events

        winrt::weak_ref<Microsoft::Terminal::Control::TermControl> _control{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _allTasks{ nullptr };

        void _runCommandButtonClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);
        void _filterTextChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        void _updateFilteredCommands();
    };

    struct FilteredTask : FilteredTaskT<FilteredTask, TerminalApp::implementation::FilteredCommand>
    {
        FilteredTask() = default;

        FilteredTask(const winrt::Microsoft::Terminal::Settings::Model::Command& command)
        {
            _constructFilteredCommand(winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(command));
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

        void UpdateFilter(const winrt::hstring& filter) override
        {
            TerminalApp::implementation::FilteredCommand::UpdateFilter(filter);
            for (const auto& c : _children)
            {
                c.UpdateFilter(filter);
            }

            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Visibility" });
        }

        winrt::hstring Input()
        {
            if (const auto& actionItem{ _Item.try_as<winrt::TerminalApp::ActionPaletteItem>() })
            {
                if (const auto& command{ actionItem.Command() })
                {
                    if (const auto& sendInput{ command.ActionAndArgs().Args().try_as<winrt::Microsoft::Terminal::Settings::Model::SendInputArgs>() })
                    {
                        return sendInput.Input();
                    }
                }
            }
            return L"";
        };

        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> Children() { return _children; }
        winrt::Microsoft::Terminal::Settings::Model::Command Command() { return _command; }

        // Used to control if this item is visible in the TreeView. Turns out,
        // TreeView is in fact sane enough to remove items entirely if they're
        // Collapsed.
        winrt::Windows::UI::Xaml::Visibility Visibility()
        {
            // Is there no filter, or do we match it?
            if (_Filter.empty() || _Weight > 0)
            {
                return winrt::Windows::UI::Xaml::Visibility::Visible;
            }
            // If we don't match, maybe one of our children does
            auto totalWeight = _Weight;
            for (const auto& c : _children)
            {
                totalWeight += c.Weight();
            }

            return totalWeight > 0 ? winrt::Windows::UI::Xaml::Visibility::Visible : winrt::Windows::UI::Xaml::Visibility::Collapsed;
        };

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _children{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TasksPaneContent);
}
