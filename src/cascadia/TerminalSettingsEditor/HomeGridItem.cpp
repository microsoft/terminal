// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HomeGridItem.h"
#include "HomeGridItem.g.cpp"

namespace winrt::SettingsControl::implementation
{
    HomeGridItem::HomeGridItem(hstring const& title, hstring const& pageTag) :
        _Title{ title },
        _PageTag{ pageTag }
    {
    }
}
