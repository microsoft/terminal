// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SearchMetadata.h"
#include "SearchMetadata.g.cpp"
#include "LibraryResources.h"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SearchMetadata::_ParentPageProperty{ nullptr };
    DependencyProperty SearchMetadata::_SettingNameProperty{ nullptr };

    SearchMetadata::SearchMetadata()
    {
        _InitializeProperties();
    }

    void SearchMetadata::_InitializeProperties()
    {
        // Initialize any dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_ParentPageProperty)
        {
            _ParentPageProperty =
                DependencyProperty::RegisterAttached(
                    L"ParentPage",
                    xaml_typename<winrt::Windows::UI::Xaml::Interop::TypeName>(),
                    xaml_typename<Editor::SearchMetadata>(),
                    PropertyMetadata{ nullptr });
        }
        if (!_SettingNameProperty)
        {
            _SettingNameProperty =
                DependencyProperty::RegisterAttached(
                    L"SettingName",
                    xaml_typename<hstring>(),
                    xaml_typename<Editor::SearchMetadata>(),
                    PropertyMetadata{ box_value(L"") });
        }
    }
}
