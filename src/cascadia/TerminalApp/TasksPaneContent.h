// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TasksPaneContent.g.h"
#include "TaskViewModel.g.h"
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

        winrt::Windows::Foundation::Size MinSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        winrt::hstring Title() { return L"Scratchpad"; }
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
        // std::vector<winrt::TerminalApp::TaskViewModel> _allCommands{};
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        void _runCommandButtonClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);
        void _filterTextChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        void _updateFilteredCommands();
    };

    struct TaskViewModel : TaskViewModelT<TaskViewModel>
    {
        TaskViewModel(const winrt::Microsoft::Terminal::Settings::Model::Command& command) :
            _command{ command }
        {
            // The Children() method must always return a non-null vector
            _children = winrt::single_threaded_observable_vector<TerminalApp::TaskViewModel>();
            if (_command.HasNestedCommands())
            {
                for (const auto& [_, child] : _command.NestedCommands())
                {
                    auto vm{ winrt::make<TaskViewModel>(child) };
                    _children.Append(vm);
                }
            }
        }

        winrt::hstring Name() { return _command.Name(); }
        winrt::hstring IconPath() { return _command.IconPath(); }
        winrt::hstring Input()
        {
            if (const auto& sendInput{ _command.ActionAndArgs().Args().try_as<winrt::Microsoft::Terminal::Settings::Model::SendInputArgs>() })
            {
                return sendInput.Input();
            }
            return L"";
        };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::TaskViewModel> Children() { return _children; }
        winrt::Microsoft::Terminal::Settings::Model::Command Command() { return _command; }

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::TaskViewModel> _children{ nullptr };
    };

    // struct FilteredTask : public winrt::TerminalApp::implementation::FilteredCommand, FilteredTaskT<FilteredTask, FilteredCommand>
    struct FilteredTask : FilteredTaskT<FilteredTask, TerminalApp::implementation::FilteredCommand>
    {
        FilteredTask() = default;

        FilteredTask(const winrt::Microsoft::Terminal::Settings::Model::Command& command)
        {
            _command = command;
            _Item = winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(command);

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

        // FilteredCommand() = default;
        // FilteredCommand(const winrt::TerminalApp::PaletteItem& item);

        // void UpdateFilter(const winrt::hstring& filter);

        // static int Compare(const winrt::TerminalApp::FilteredCommand& first, const winrt::TerminalApp::FilteredCommand& second);

        // WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        // WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::PaletteItem, Item, _PropertyChangedHandlers, nullptr);
        // WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Filter, _PropertyChangedHandlers);
        // WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::HighlightedText, HighlightedName, _PropertyChangedHandlers);
        // WINRT_OBSERVABLE_PROPERTY(int, Weight, _PropertyChangedHandlers);

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

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _children{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TasksPaneContent);
    BASIC_FACTORY(TaskViewModel);
}
