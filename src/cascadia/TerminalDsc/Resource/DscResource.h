// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ResourceMetadata.h"

namespace Microsoft::Terminal::Dsc
{
    // Exit codes shared by every resource served from this executable. These
    // are the floor; each resource documents its full set in its
    // ResourceMetadata and thereby in the generated manifest.
    inline constexpr int ExitSuccess = 0;
    inline constexpr int ExitError = 1;
    inline constexpr int ExitInvalidInput = 2;
    inline constexpr int ExitResourceError = 3;

    // The desired state handed to us was malformed (bad JSON, wrong property
    // types, unknown properties). Maps to ExitInvalidInput.
    struct DscInputError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // Reading or writing the actual state failed. Maps to ExitResourceError.
    struct DscResourceError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    // A Microsoft DSC resource served by this executable. Which operations a
    // resource supports is expressed by additionally deriving from the
    // capability interfaces below; the registry detects them at compile time
    // during registration, and both the command dispatcher and the manifest
    // generator read from that single source of truth.
    struct IDscResource
    {
        virtual ~IDscResource() = default;

        virtual const ResourceMetadata& Metadata() const noexcept = 0;

        // Returns the JSON schema describing the resource's properties. This
        // is the source of truth for the manifest's embedded schema.
        virtual Json::Value Schema() const = 0;
    };

    // Capability: "get". The instance carries any key/filter properties the
    // caller provided; resources without key properties may ignore it.
    struct IGettable
    {
        virtual ~IGettable() = default;
        virtual Json::Value Get(const std::optional<Json::Value>& instance) = 0;
    };

    // Capability: "set". Returns the final state (SetReturn::State) which the
    // executor writes to stdout.
    struct ISettable
    {
        virtual ~ISettable() = default;
        virtual Json::Value Set(const Json::Value& instance) = 0;
    };

    // Capability: "test". Only needed when DSC's synthetic test (get + diff)
    // is not good enough for the resource's semantics.
    struct ITestable
    {
        virtual ~ITestable() = default;
        virtual Json::Value Test(const Json::Value& instance) = 0;
    };

    // Capability: "delete".
    struct IDeletable
    {
        virtual ~IDeletable() = default;
        virtual void Delete(const std::optional<Json::Value>& instance) = 0;
    };

    // Capability: "export". Returns all instances; single-instance resources
    // return exactly their current state.
    struct IExportable
    {
        virtual ~IExportable() = default;
        virtual std::vector<Json::Value> Export(const std::optional<Json::Value>& filter) = 0;
    };
}
