// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ResourceAccessor.h"

using namespace ::winrt::Windows::ApplicationModel::Resources::Core;

static constexpr std::wstring_view resourceLocatorBase{ L"TerminalApp/Resources" };

// Method Description:
// - Gets the resource map associated with the TerminalApp resource subcompartment.
// Return Value:
// - the resource map associated with the TerminalApp resource subcompartment.
ResourceMap ResourceAccessor::GetResourceMap()
{
    auto packageResourceMap = ResourceManager::Current().MainResourceMap();
    return packageResourceMap.GetSubtree(resourceLocatorBase);
}

// Method Description:
// - Loads the localized string resource with the given key from the TerminalApp
//   resource subcompartment.
// - This resource loader is view-independent; it cannot take into account scale
//   factors and view themes. Strings should not vary based on those things.
// Arguments:
// - resourceName: the key up by which to look the resource
// Return Value:
// - The final localized string for the given key.
winrt::hstring ResourceAccessor::GetLocalizedString(const std::wstring_view& resourceName)
{
    static ResourceMap s_resourceMap = GetResourceMap();
    static ResourceContext s_resourceContext = ResourceContext::GetForViewIndependentUse();

    return s_resourceMap.GetValue(resourceName, s_resourceContext).ValueAsString();
}
