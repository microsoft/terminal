// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "LaunchViewModel.g.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct LaunchViewModel : LaunchViewModelT<LaunchViewModel>, ViewModelHelper<LaunchViewModel>
    {
        LaunchViewModel() = default;
        winrt::hstring Ayy() { return L"lmao"; };
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    //BASIC_FACTORY(LaunchViewModel);
}
