// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "TabViewItemTemplateSettings.g.h"
#include "TabViewItemTemplateSettings.properties.h"

class TabViewItemTemplateSettings :
    public winrt::implementation::TabViewItemTemplateSettingsT<TabViewItemTemplateSettings>,
    public TabViewItemTemplateSettingsProperties
{
public:
    TabViewItemTemplateSettings() { EnsureProperties();  }
};
