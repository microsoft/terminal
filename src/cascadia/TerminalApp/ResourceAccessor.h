// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

class ResourceAccessor
{
public:
    static winrt::Windows::ApplicationModel::Resources::Core::ResourceMap GetResourceMap();
    static winrt::hstring GetLocalizedString(const std::wstring_view& resourceName);

private:
    ResourceAccessor() = delete;
};
