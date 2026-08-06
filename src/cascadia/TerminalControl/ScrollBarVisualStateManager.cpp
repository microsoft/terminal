// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ScrollBarVisualStateManager.h"
#include "ScrollBarVisualStateManager.g.cpp"

#include "TermControl.h"

using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    bool ScrollBarVisualStateManager::GoToStateCore(
        winrt::Windows::UI::Xaml::Controls::Control const& control,
        winrt::Windows::UI::Xaml::FrameworkElement const& templateRoot,
        winrt::hstring const& stateName,
        winrt::Windows::UI::Xaml::VisualStateGroup const& group,
        winrt::Windows::UI::Xaml::VisualState const& state,
        bool useTransitions)
    {
        if (!_initialized)
        {
            _initialized = true;

            Control::TermControl termControl{ nullptr };

            for (auto parent = VisualTreeHelper::GetParent(control); parent; parent = VisualTreeHelper::GetParent(parent))
            {
                if (parent.try_as(termControl))
                {
                    _termControl = winrt::get_self<TermControl>(termControl)->get_weak();
                    break;
                }
            }

            assert(termControl);
        }

        if (const auto termControl = _termControl.get())
        {
            const auto scrollState = termControl->Settings().ScrollState();
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
        }

        return base_type::GoToStateCore(control, templateRoot, stateName, group, state, useTransitions);
    }
}
