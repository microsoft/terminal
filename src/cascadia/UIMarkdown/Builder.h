// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Builder.g.h"

namespace winrt::Microsoft::Terminal::UI::Markdown::implementation
{
    struct Builder
    {
        static winrt::Windows::UI::Xaml::Controls::RichTextBlock Convert(const winrt::hstring& text, const winrt::hstring& baseUrl);
    };
}

namespace winrt::Microsoft::Terminal::UI::Markdown::factory_implementation
{
    BASIC_FACTORY(Builder);
}
