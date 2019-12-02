// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorFlyoutSubItem.h"
#include "Utils.h"

#include "ColorFlyoutSubItem.g.cpp"
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::ColorFlyoutSubItem::implementation
{
    ColorFlyoutSubItem::ColorFlyoutSubItem() {}

    ColorFlyoutSubItem::ColorFlyoutSubItem(std::shared_ptr<ScopedResourceLoader> resourceLoader)
    {
        InitializeComponent();

        _resourceLoader = resourceLoader;
    }

    void ColorFlyoutSubItem::Create()
    {
    }
}
