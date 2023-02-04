// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItemTemplateSelector.g.h"

namespace winrt::TerminalApp::implementation
{
    struct PaletteItemTemplateSelector : PaletteItemTemplateSelectorT<PaletteItemTemplateSelector>
    {
        PaletteItemTemplateSelector() = default;

        WUX::DataTemplate SelectTemplateCore(const WF::IInspectable&, const WUX::DependencyObject&);
        WUX::DataTemplate SelectTemplateCore(const WF::IInspectable&);

        WINRT_PROPERTY(WUX::DataTemplate, TabItemTemplate);
        WINRT_PROPERTY(WUX::DataTemplate, NestedItemTemplate);
        WINRT_PROPERTY(WUX::DataTemplate, GeneralItemTemplate);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(PaletteItemTemplateSelector);
}
