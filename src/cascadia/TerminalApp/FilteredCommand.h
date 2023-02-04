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
        FilteredCommand(const MTApp::PaletteItem& item);

        void UpdateFilter(const winrt::hstring& filter);

        static int Compare(const MTApp::FilteredCommand& first, const MTApp::FilteredCommand& second);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(MTApp::PaletteItem, Item, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Filter, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(MTApp::HighlightedText, HighlightedName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int, Weight, _PropertyChangedHandlers);

    private:
        MTApp::HighlightedText _computeHighlightedName();
        int _computeWeight();
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _itemChangedRevoker;

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
