// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AdaptiveCardContent.h"
#include "AdaptiveCardContent.g.cpp"

namespace winrt::TerminalApp::implementation
{
    winrt::Windows::UI::Xaml::FrameworkElement AdaptiveCardContent::GetRoot()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Size AdaptiveCardContent::MinSize()
    {
        return Windows::Foundation::Size{ 1, 1 };
    }
    void AdaptiveCardContent::Focus()
    {
        throw hresult_not_implemented();
    }
    void AdaptiveCardContent::Close()
    {
        throw hresult_not_implemented();
    }
}
