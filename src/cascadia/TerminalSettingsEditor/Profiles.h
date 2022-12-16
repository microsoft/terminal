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
                                   const Editor::ColorSchemesPageViewModel& schemesPageVM,
                                   const IHostedInWindow& windowRoot) :
            _Profile{ viewModel }
        {
            auto profile{ winrt::get_self<ProfileViewModel>(viewModel) };
            profile->SchemesPageVM(schemesPageVM);
            profile->WindowRoot(windowRoot);

            viewModel.DefaultAppearance().SchemesPageVM(schemesPageVM);
            viewModel.DefaultAppearance().WindowRoot(windowRoot);

            if (viewModel.UnfocusedAppearance())
            {
                viewModel.UnfocusedAppearance().SchemesPageVM(schemesPageVM);
                viewModel.UnfocusedAppearance().WindowRoot(windowRoot);
            }
        }
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);
        WINRT_PROPERTY(bool, FocusDeleteButton, false);
    };
};
