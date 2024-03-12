// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CodeBlock.g.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"
#include <til/hash.h>

namespace winrt::SampleApp::implementation
{
    struct CodeBlock : CodeBlockT<CodeBlock>
    {
        CodeBlock(const winrt::hstring& initialCommandlines);

        til::property_changed_event PropertyChanged;

    private:
        friend struct CodeBlockT<CodeBlock>; // for Xaml to bind events

        winrt::hstring _providedCommandlines{};

        void _playPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(CodeBlock);
}
