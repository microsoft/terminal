// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "ColorFlyoutSubItem.g.h"
#include "ScopedResourceLoader.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

namespace winrt::ColorFlyoutSubItem::implementation
{
    struct ColorFlyoutSubItem : ColorFlyoutSubItemT<ColorFlyoutSubItem>
    {
    public:
        ColorFlyoutSubItem();

        ColorFlyoutSubItem(std::shared_ptr<ScopedResourceLoader> resourceLoader);
        void Create();

    private:
        std::shared_ptr<ScopedResourceLoader> _resourceLoader{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ColorFlyoutSubItem : ColorFlyoutSubItemT<ColorFlyoutSubItem, implementation::ColorFlyoutSubItem>
    {
    };
}
