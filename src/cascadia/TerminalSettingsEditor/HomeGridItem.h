// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "HomeGridItem.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct HomeGridItem : HomeGridItemT<HomeGridItem>
    {
        HomeGridItem() = delete;
        HomeGridItem(hstring const& title, hstring const& pageTag);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(hstring, Title, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, PageTag, _PropertyChangedHandlers);
    };
}
