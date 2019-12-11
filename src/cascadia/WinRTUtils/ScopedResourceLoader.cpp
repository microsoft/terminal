// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ScopedResourceLoader.h"

using namespace ::winrt::Windows::ApplicationModel::Resources::Core;

ScopedResourceLoader::ScopedResourceLoader(const std::wstring_view resourceLocatorBase) :
    _resourceMap{ ResourceManager::Current().MainResourceMap().GetSubtree(resourceLocatorBase) },
    _resourceContext{ ResourceContext::GetForViewIndependentUse() }
{
}

// Method Description:
// - Gets the resource map associated with the scoped resource subcompartment.
// Return Value:
// - the resource map associated with the scoped resource subcompartment.
ResourceMap ScopedResourceLoader::GetResourceMap() const
{
    return _resourceMap;
}

// Method Description:
// - Loads the localized string resource with the given key from the scoped
//   resource subcompartment.
// - This resource loader is view-independent; it cannot take into account scale
//   factors and view themes. Strings should not vary based on those things.
// Arguments:
// - resourceName: the key up by which to look the resource
// Return Value:
// - The final localized string for the given key.
winrt::hstring ScopedResourceLoader::GetLocalizedString(const std::wstring_view resourceName) const
{
    return _resourceMap.GetValue(resourceName, _resourceContext).ValueAsString();
}

// Method Description:
// - Returns whether this resource loader can find a resource with the given key.
// Arguments:
// - resourceName: the key up by which to look the resource
// Return Value:
// - A boolean indicating whether the resource was found
bool ScopedResourceLoader::HasResourceWithName(const std::wstring_view resourceName) const
{
    return _resourceMap.HasKey(resourceName);
}
