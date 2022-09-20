// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ScrollBarVisualStateManager.h"
#include "ScrollBarVisualStateManager.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    ScrollBarVisualStateManager::ScrollBarVisualStateManager() :
        _termControl(nullptr)
    {
    }

    bool ScrollBarVisualStateManager::GoToStateCore(
        winrt::Windows::UI::Xaml::Controls::Control const& control,
        winrt::Windows::UI::Xaml::FrameworkElement const& templateRoot,
        winrt::hstring const& stateName,
        winrt::Windows::UI::Xaml::VisualStateGroup const& group,
        winrt::Windows::UI::Xaml::VisualState const& state,
        bool useTransitions)
    {
        if (!_termControl)
        {
            for (auto parent = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(control);
                 parent != nullptr;
                 parent = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(parent))
            {
                if (parent.try_as(_termControl))
                {
                    break;
                }
            }
        }

        WINRT_ASSERT(_termControl);

        auto scrollState = _termControl.Settings().ScrollState();
        if (scrollState == ScrollbarState::Always)
        {
            // If we're in Always mode, and the control is trying to collapse,
            // go back to expanded
            if (stateName == L"Collapsed" || stateName == L"CollapsedWithoutAnimation")
            {
                for (auto foundState : group.States())
                {
                    if (foundState.Name() == L"ExpandedWithoutAnimation")
                    {
                        return base_type::GoToStateCore(control, templateRoot, foundState.Name(), group, foundState, false);
                    }
                }
            }
        }

        return base_type::GoToStateCore(control, templateRoot, stateName, group, state, useTransitions);
    }
}
