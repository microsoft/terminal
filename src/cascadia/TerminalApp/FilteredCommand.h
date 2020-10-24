// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextControl.h"
#include "FilteredCommand.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class FilteredCommandTests;
};

namespace winrt::TerminalApp::implementation
{
    struct FilteredCommand : FilteredCommandT<FilteredCommand>
    {
        FilteredCommand() = default;
        FilteredCommand(Microsoft::Terminal::Settings::Model::Command const& command);

        void UpdateFilter(winrt::hstring const& filter);

        static int Compare(winrt::TerminalApp::FilteredCommand const& first, winrt::TerminalApp::FilteredCommand const& second);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(Microsoft::Terminal::Settings::Model::Command, Command, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Filter, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::HighlightedText, HighlightedName, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(int, Weight, _PropertyChangedHandlers);

    private:
        winrt::TerminalApp::HighlightedText _computeHighlightedName();
        int _computeWeight();
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _commandChangedRevoker;

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
