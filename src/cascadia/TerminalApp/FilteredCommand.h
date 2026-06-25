// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HighlightedTextControl.h"
#include "FilteredCommand.g.h"
#include "fzf/fzf.h"

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
        FilteredCommand(const winrt::TerminalApp::IPaletteItem& item);

        void UpdateFilter(std::shared_ptr<fzf::matcher::Pattern> pattern);

        static int Compare(const winrt::TerminalApp::FilteredCommand& first, const winrt::TerminalApp::FilteredCommand& second);

        bool HasSubtitle();

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::IPaletteItem, Item, PropertyChanged.raise, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun>, NameHighlights, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun>, SubtitleHighlights, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(int, Weight, PropertyChanged.raise);

    private:
        std::shared_ptr<fzf::matcher::Pattern> _pattern;
        void _update();
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _itemChangedRevoker;

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
