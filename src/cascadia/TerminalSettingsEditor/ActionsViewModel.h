// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ActionsViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ActionsViewModel : ActionsViewModelT<ActionsViewModel>, ViewModelHelper<ActionsViewModel>
    {
    public:
        ActionsViewModel();

        winrt::hstring Name();

    private:
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ActionsViewModel);
}
