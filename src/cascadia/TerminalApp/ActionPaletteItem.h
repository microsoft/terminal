// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "ActionPaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionPaletteItem : ActionPaletteItemT<ActionPaletteItem, PaletteItem>
    {
        ActionPaletteItem() = default;
        ActionPaletteItem(const Microsoft::Terminal::Settings::Model::Command& command);

        WINRT_PROPERTY(Microsoft::Terminal::Settings::Model::Command, Command, nullptr);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _commandChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionPaletteItem);
}
