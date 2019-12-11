// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

class ScopedResourceLoader
{
public:
    ScopedResourceLoader(const std::wstring_view resourceLocatorBase);
    winrt::Windows::ApplicationModel::Resources::Core::ResourceMap GetResourceMap() const;
    winrt::hstring GetLocalizedString(const std::wstring_view resourceName) const;
    bool HasResourceWithName(const std::wstring_view resourceName) const;

private:
    winrt::Windows::ApplicationModel::Resources::Core::ResourceMap _resourceMap;
    winrt::Windows::ApplicationModel::Resources::Core::ResourceContext _resourceContext;
};
