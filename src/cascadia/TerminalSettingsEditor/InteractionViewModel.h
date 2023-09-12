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
        InteractionViewModel(Model::WindowSettings windowSettings);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<InteractionViewModel>::PropertyChanged;

        GETSET_BINDABLE_ENUM_SETTING(TabSwitcherMode, Model::TabSwitcherMode, _windowSettings.TabSwitcherMode);
        GETSET_BINDABLE_ENUM_SETTING(CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, _windowSettings.CopyFormatting);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, CopyOnSelect);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, TrimBlockSelection);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, TrimPaste);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, SnapToGridOnResize);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, FocusFollowMouse);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, DetectURLs);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, WordDelimiters);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, ConfirmCloseAllTabs);

    private:
        Model::WindowSettings _windowSettings;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(InteractionViewModel);
}
