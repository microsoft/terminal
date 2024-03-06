// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextControl.h"
#include "FilteredCommand.g.h"

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
        FilteredCommand(const winrt::TerminalApp::PaletteItem& item);

        void UpdateFilter(const winrt::hstring& filter);

        static int Compare(const winrt::TerminalApp::FilteredCommand& first, const winrt::TerminalApp::FilteredCommand& second);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::PaletteItem, Item, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Filter, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::HighlightedText, HighlightedName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int, Weight, _PropertyChangedHandlers);

    private:
        winrt::TerminalApp::HighlightedText _computeHighlightedName();
        int _computeWeight();
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _itemChangedRevoker;

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
