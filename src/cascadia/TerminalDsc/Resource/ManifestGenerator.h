// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ResourceRegistry.h"

namespace Microsoft::Terminal::Dsc
{
    // Generates Microsoft DSC resource manifests from the registry, so the
    // manifest can never drift from what the executable actually implements.
    namespace ManifestGenerator
    {
        // The manifest for a single registration. When singleResource is true
        // the --resource selector is omitted from the generated args.
        Json::Value BuildResourceManifest(const ResourceRegistration& registration, bool singleResource, std::string_view executable);

        // The full manifest document: the plain resource manifest when one
        // resource is registered.
        Json::Value BuildManifestDocument(const ResourceRegistry& registry, std::string_view executable);

        // The conventional file name for the manifest document:
        // "<type lowercased, '/'→'.'>.dsc.resource.json" for a single
        // resource, "<exe stem>.dsc.manifests.json" for several.
        std::string ManifestFileName(const ResourceRegistry& registry, std::string_view executableStem);
    }
}
