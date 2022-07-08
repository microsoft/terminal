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

        GETSET_BINDABLE_ENUM_SETTING(TabSwitcherMode, Model::TabSwitcherMode, _GlobalSettings.TabSwitcherMode);
        GETSET_BINDABLE_ENUM_SETTING(CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, _GlobalSettings.CopyFormatting);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, CopyOnSelect);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, TrimBlockSelection);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, TrimPaste);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, SnapToGridOnResize);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, FocusFollowMouse);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, DetectURLs);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, WordDelimiters);

    private:
        Model::GlobalAppSettings _GlobalSettings;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(InteractionViewModel);
}
