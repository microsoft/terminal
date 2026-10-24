// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ResourceRegistry.h"

namespace Microsoft::Terminal::Dsc
{
    namespace
    {
        bool equalsInsensitive(std::string_view left, std::string_view right) noexcept
        {
            return left.size() == right.size() &&
                   _strnicmp(left.data(), right.data(), left.size()) == 0;
        }
    }

    void ResourceRegistry::_add(ResourceRegistration&& registration)
    {
        const auto type{ registration.Metadata().type };
        if (Find(type))
        {
            throw std::invalid_argument{ fmt::format(FMT_COMPILE("resource '{}' is already registered"), type) };
        }
        _registrations.emplace_back(std::move(registration));
    }

    const ResourceRegistration* ResourceRegistry::Find(std::string_view type) const noexcept
    {
        for (const auto& registration : _registrations)
        {
            if (equalsInsensitive(registration.Metadata().type, type))
            {
                return &registration;
            }
        }
        return nullptr;
    }
}
