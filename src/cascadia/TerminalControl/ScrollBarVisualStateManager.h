// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ScrollBarVisualStateManager.h
//
// Abstract:
// - This is a custom VisualStateManager that keeps the scroll bar from
//   going into the collapsed mode when the user wants the scroll bar
//   to always be displayed.
//

#pragma once
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>

#include "ScrollBarVisualStateManager.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct TermControl;

    struct ScrollBarVisualStateManager : ScrollBarVisualStateManagerT<ScrollBarVisualStateManager>
    {
        bool GoToStateCore(winrt::Windows::UI::Xaml::Controls::Control const& control, winrt::Windows::UI::Xaml::FrameworkElement const& templateRoot, hstring const& stateName, winrt::Windows::UI::Xaml::VisualStateGroup const& group, winrt::Windows::UI::Xaml::VisualState const& state, bool useTransitions);

    private:
        winrt::weak_ref<TermControl> _termControl;
        bool _initialized = false;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    struct ScrollBarVisualStateManager : ScrollBarVisualStateManagerT<ScrollBarVisualStateManager, implementation::ScrollBarVisualStateManager>
    {
    };
}
