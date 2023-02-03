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

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct FilteredCommand : FilteredCommandT<FilteredCommand>
    {
        FilteredCommand() = default;
        FilteredCommand(const winrt::Microsoft::Terminal::App::PaletteItem& item);

        void UpdateFilter(const winrt::hstring& filter);

        static int Compare(const winrt::Microsoft::Terminal::App::FilteredCommand& first, const winrt::Microsoft::Terminal::App::FilteredCommand& second);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::Microsoft::Terminal::App::PaletteItem, Item, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Filter, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::Microsoft::Terminal::App::HighlightedText, HighlightedName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int, Weight, _PropertyChangedHandlers);

    private:
        winrt::Microsoft::Terminal::App::HighlightedText _computeHighlightedName();
        int _computeWeight();
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _itemChangedRevoker;

        friend class TerminalAppLocalTests::FilteredCommandTests;
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(FilteredCommand);
}
