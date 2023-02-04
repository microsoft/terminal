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

#include "TermControl.h"

#include "ScrollBarVisualStateManager.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ScrollBarVisualStateManager : ScrollBarVisualStateManagerT<ScrollBarVisualStateManager>
    {
        bool GoToStateCore(WUXC::Control const& control, WUX::FrameworkElement const& templateRoot, hstring const& stateName, WUX::VisualStateGroup const& group, WUX::VisualState const& state, bool useTransitions);

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
