// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "InteractionViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct InteractionViewModel : InteractionViewModelT<InteractionViewModel>, ViewModelHelper<InteractionViewModel>
    {
    public:
        InteractionViewModel(Model::GlobalAppSettings globalSettings);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Model::GlobalAppSettings, GlobalSettings, nullptr)

        GETSET_BINDABLE_ENUM_SETTING(TabSwitcherMode, Model::TabSwitcherMode, _GlobalSettings.TabSwitcherMode);
        GETSET_BINDABLE_ENUM_SETTING(CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, _GlobalSettings.CopyFormatting);
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(InteractionViewModel);
}
