// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "FilteredTask.g.h"
#include "FilteredCommand.h"
#include "ActionPaletteItem.h"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
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

        static int Compare(const winrt::TerminalApp::FilteredTask& first, const winrt::TerminalApp::FilteredTask& second)
        {
            auto firstWeight{ winrt::get_self<implementation::FilteredTask>(first)->Weight() };
            auto secondWeight{ winrt::get_self<implementation::FilteredTask>(second)->Weight() };

            if (firstWeight == secondWeight)
            {
                std::wstring_view firstName{ first.FilteredCommand().Item().Name() };
                std::wstring_view secondName{ second.FilteredCommand().Item().Name() };
                return lstrcmpi(firstName.data(), secondName.data()) < 0;
            }

            return firstWeight > secondWeight;
        }

        void UpdateFilter(const winrt::hstring& filter)
        {
            _filteredCommand->UpdateFilter(filter);
            HighlightedInput(FilteredCommand::ComputeHighlighted(Input(), filter));

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

        winrt::TerminalApp::PaletteItem Item() { return _filteredCommand->Item(); }

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
            const auto ourWeight = Weight();
            // Is there no filter, or do we match it?
            if (_filteredCommand->Filter().empty() || ourWeight > 0)
            {
                return winrt::Windows::UI::Xaml::Visibility::Visible;
            }
            // If we don't match, maybe one of our children does
            auto totalWeight = ourWeight;
            for (const auto& c : _children)
            {
                auto impl = winrt::get_self<implementation::FilteredTask>(c);
                totalWeight += impl->Weight();
            }

            return totalWeight > 0 ? winrt::Windows::UI::Xaml::Visibility::Visible : winrt::Windows::UI::Xaml::Visibility::Collapsed;
        };

        int Weight()
        {
            return std::max(_filteredCommand->Weight(), _HighlightedInput.Weight());
        }

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::HighlightedText, HighlightedInput, PropertyChanged.raise);

    private:
        winrt::Microsoft::Terminal::Settings::Model::Command _command{ nullptr };
        winrt::com_ptr<winrt::TerminalApp::implementation::FilteredCommand> _filteredCommand{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::FilteredTask> _children{ nullptr };
    };
}
