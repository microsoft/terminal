// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "AdaptiveCardContent.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct AdaptiveCardContent : AdaptiveCardContentT<AdaptiveCardContent>
    {
        AdaptiveCardContent();
        bool InitFromString(const winrt::hstring& jsonString);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();
        winrt::Windows::Foundation::Size MinSize();
        void Focus();
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetTerminalArgsForPane() const;

    private:
        winrt::Windows::UI::Xaml::FrameworkElement _root{ nullptr };
        winrt::AdaptiveCards::Rendering::Uwp::RenderedAdaptiveCard _renderedCard{ nullptr };
    };
}
