// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "ActionPaletteItem.g.h"
#include "inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionPaletteItem : ActionPaletteItemT<ActionPaletteItem, PaletteItem>
    {
        ActionPaletteItem() = default;
        ActionPaletteItem(Microsoft::Terminal::Settings::Model::Command const& command);

        GETSET_PROPERTY(Microsoft::Terminal::Settings::Model::Command, Command, nullptr);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _commandChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ActionPaletteItem);
}
