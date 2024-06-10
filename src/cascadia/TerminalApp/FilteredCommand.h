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
        static winrt::TerminalApp::HighlightedText ComputeHighlighted(winrt::hstring input, winrt::hstring filter);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::PaletteItem, Item, PropertyChanged.raise, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Filter, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::HighlightedText, HighlightedName, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(int, Weight, PropertyChanged.raise);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _itemChangedRevoker;

        void _constructFilteredCommand(const winrt::TerminalApp::PaletteItem& item);
        winrt::TerminalApp::HighlightedText _computeHighlighted(winrt::hstring input);
        int _computeWeight();

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
