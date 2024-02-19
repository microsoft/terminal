// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TasksPaneContent.g.h"
#include "TaskViewModel.g.h"

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

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

    private:
        friend struct TasksPaneContentT<TasksPaneContent>; // for Xaml to bind events

        // winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        // winrt::Microsoft::UI::Xaml::Controls::TreeView _treeView{ nullptr };

        void _containerContentChanging(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args);
    };

    struct TaskViewModel : TaskViewModelT<TaskViewModel>
    {
        TaskViewModel(const winrt::Microsoft::Terminal::Settings::Model::Command& command) :
            _command{ command }
        {
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
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TasksPaneContent);
    BASIC_FACTORY(TaskViewModel);
}
