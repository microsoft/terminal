// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "DscResource.h"

namespace Microsoft::Terminal::Dsc
{
    // Everything the dispatcher and the manifest generator need to know about
    // one registered resource. The capability pointers are captured from the
    // concrete type when the resource is added; a null pointer means the
    // resource does not support that operation.
    struct ResourceRegistration
    {
        std::unique_ptr<IDscResource> resource;
        IGettable* get = nullptr;
        ISettable* set = nullptr;
        ITestable* test = nullptr;
        IDeletable* del = nullptr;
        IExportable* exp = nullptr;

        const ResourceMetadata& Metadata() const noexcept
        {
            return resource->Metadata();
        }
    };

    // The registry of resources served by this executable, keyed on the fully
    // qualified resource type (case-insensitive). Add new resources in
    // main.cpp.
    class ResourceRegistry
    {
    public:
        // Registers a resource. Which capability interfaces the concrete type
        // implements is determined at compile time; the repo
        // builds without RTTI, so this cannot be a dynamic_cast. Throws
        // std::invalid_argument when a resource with the same type is already
        // registered.
        template<typename T>
        void Add(std::unique_ptr<T> resource)
        {
            static_assert(std::is_base_of_v<IDscResource, T>, "resources must implement IDscResource");
            ResourceRegistration registration;
            if constexpr (std::is_base_of_v<IGettable, T>)
            {
                registration.get = resource.get();
            }
            if constexpr (std::is_base_of_v<ISettable, T>)
            {
                registration.set = resource.get();
            }
            if constexpr (std::is_base_of_v<ITestable, T>)
            {
                registration.test = resource.get();
            }
            if constexpr (std::is_base_of_v<IDeletable, T>)
            {
                registration.del = resource.get();
            }
            if constexpr (std::is_base_of_v<IExportable, T>)
            {
                registration.exp = resource.get();
            }
            registration.resource = std::move(resource);
            _add(std::move(registration));
        }

        // Case-insensitive lookup; returns nullptr when not found.
        const ResourceRegistration* Find(std::string_view type) const noexcept;

        size_t Count() const noexcept
        {
            return _registrations.size();
        }

        // When exactly one resource is registered, the --resource selector is
        // implicit (and omitted from the generated manifest args).
        bool IsSingleResource() const noexcept
        {
            return _registrations.size() == 1;
        }

        const std::vector<ResourceRegistration>& All() const noexcept
        {
            return _registrations;
        }

    private:
        void _add(ResourceRegistration&& registration);

        std::vector<ResourceRegistration> _registrations;
    };
}
