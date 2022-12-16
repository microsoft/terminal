// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ProfilePageNavigationState.g.h"
#include "ProfileViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "Appearances.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ProfilePageNavigationState : ProfilePageNavigationStateT<ProfilePageNavigationState>
    {
    public:
        ProfilePageNavigationState(const Editor::ProfileViewModel& viewModel,
                                   const IHostedInWindow& windowRoot) :
            _Profile{ viewModel }
        {
            auto profile{ winrt::get_self<ProfileViewModel>(viewModel) };
            profile->WindowRoot(windowRoot);

            viewModel.DefaultAppearance().WindowRoot(windowRoot);

            if (viewModel.UnfocusedAppearance())
            {
                viewModel.UnfocusedAppearance().WindowRoot(windowRoot);
            }
        }
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
    };
};
