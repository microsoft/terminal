/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SearchMetadata

Abstract:
- A custom XAML attached property (like AutomationProperties)
  to hold metadata for search in the settings editor.

Author(s):
- Carlos Zamora - October 2025

--*/

#pragma once

#include "SearchMetadata.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SearchMetadata : SearchMetadataT<SearchMetadata>
    {
    public:
        SearchMetadata();

        ATTACHED_DEPENDENCY_PROPERTY(winrt::Windows::UI::Xaml::Interop::TypeName, ParentPage);
        ATTACHED_DEPENDENCY_PROPERTY(winrt::hstring, SettingName);

    private:
        static void _InitializeProperties();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SearchMetadata);
}
