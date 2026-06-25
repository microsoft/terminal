// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

class ScopedResourceLoader
{
public:
    ScopedResourceLoader(const std::wstring_view resourceLocatorBase);
    winrt::Windows::ApplicationModel::Resources::Core::ResourceMap GetResourceMap() const noexcept;
    winrt::hstring GetLocalizedString(const std::wstring_view resourceName) const;
    bool HasResourceWithName(const std::wstring_view resourceName) const;

    ScopedResourceLoader WithQualifier(const wil::zwstring_view qualifierName, const wil::zwstring_view qualifierValue) const;

    const winrt::Windows::ApplicationModel::Resources::Core::ResourceMap& ResourceMap() const noexcept { return _resourceMap; }
    const winrt::Windows::ApplicationModel::Resources::Core::ResourceContext& ResourceContext() const noexcept { return _resourceContext; }

private:
    ScopedResourceLoader(winrt::Windows::ApplicationModel::Resources::Core::ResourceMap map, winrt::Windows::ApplicationModel::Resources::Core::ResourceContext context) noexcept;
    winrt::Windows::ApplicationModel::Resources::Core::ResourceMap _resourceMap;
    winrt::Windows::ApplicationModel::Resources::Core::ResourceContext _resourceContext;
};
