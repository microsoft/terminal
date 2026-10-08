// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ManifestGenerator.h"

namespace Microsoft::Terminal::Dsc::ManifestGenerator
{
    namespace
    {
        Json::Value buildArgs(std::string_view verb, const ResourceRegistration& registration, bool singleResource, bool inputMandatory)
        {
            Json::Value args{ Json::arrayValue };
            args.append(std::string{ verb });
            if (!singleResource)
            {
                args.append("--resource");
                args.append(std::string{ registration.Metadata().type });
            }
            Json::Value inputArg{ Json::objectValue };
            inputArg["jsonInputArg"] = "--input";
            inputArg["mandatory"] = inputMandatory;
            args.append(inputArg);
            return args;
        }

        Json::Value buildMethod(std::string_view verb, const ResourceRegistration& registration, bool singleResource, std::string_view executable, bool inputMandatory)
        {
            Json::Value method{ Json::objectValue };
            method["executable"] = std::string{ executable };
            method["args"] = buildArgs(verb, registration, singleResource, inputMandatory);
            return method;
        }

        std::string_view setReturnString(SetReturn value)
        {
            switch (value)
            {
            case SetReturn::State:
                return "state";
            case SetReturn::StateAndDiff:
                return "stateAndDiff";
            default:
                return {};
            }
        }
    }

    Json::Value BuildResourceManifest(const ResourceRegistration& registration, bool singleResource, std::string_view executable)
    {
        const auto& metadata{ registration.Metadata() };

        Json::Value manifest{ Json::objectValue };
        manifest["$schema"] = "https://aka.ms/dsc/schemas/v3/bundled/resource/manifest.json";
        manifest["type"] = std::string{ metadata.type };
        manifest["version"] = std::string{ metadata.version };
        manifest["description"] = std::string{ metadata.description };

        if (!metadata.tags.empty())
        {
            Json::Value tags{ Json::arrayValue };
            for (const auto& tag : metadata.tags)
            {
                tags.append(std::string{ tag });
            }
            manifest["tags"] = tags;
        }

        if (!metadata.exitCodes.empty())
        {
            Json::Value exitCodes{ Json::objectValue };
            for (const auto& exitCode : metadata.exitCodes)
            {
                exitCodes[std::to_string(exitCode.code)] = std::string{ exitCode.description };
            }
            manifest["exitCodes"] = exitCodes;
        }

        if (registration.get)
        {
            manifest["get"] = buildMethod("get", registration, singleResource, executable, metadata.getRequiresInput);
        }
        if (registration.set)
        {
            auto method{ buildMethod("set", registration, singleResource, executable, true) };
            if (const auto returnKind{ setReturnString(metadata.setReturn) }; !returnKind.empty())
            {
                method["return"] = std::string{ returnKind };
            }
            manifest["set"] = method;
        }
        if (registration.test)
        {
            manifest["test"] = buildMethod("test", registration, singleResource, executable, true);
        }
        if (registration.del)
        {
            manifest["delete"] = buildMethod("delete", registration, singleResource, executable, true);
        }
        if (registration.exp)
        {
            manifest["export"] = buildMethod("export", registration, singleResource, executable, false);
        }

        Json::Value schema{ Json::objectValue };
        schema["embedded"] = registration.resource->Schema();
        manifest["schema"] = schema;

        return manifest;
    }

    Json::Value BuildManifestDocument(const ResourceRegistry& registry, std::string_view executable)
    {
        if (registry.IsSingleResource())
        {
            return BuildResourceManifest(registry.All().front(), true, executable);
        }

        Json::Value document{ Json::objectValue };
        Json::Value resources{ Json::arrayValue };
        for (const auto& registration : registry.All())
        {
            resources.append(BuildResourceManifest(registration, false, executable));
        }
        document["resources"] = resources;
        return document;
    }

    std::string ManifestFileName(const ResourceRegistry& registry, std::string_view executableStem)
    {
        if (registry.IsSingleResource())
        {
            auto name{ std::string{ registry.All().front().Metadata().type } };
            std::transform(name.begin(), name.end(), name.begin(), [](char ch) {
                return ch == '/' ? '.' : til::tolower_ascii(ch);
            });
            return name + ".dsc.resource.json";
        }
        return std::string{ executableStem } + ".dsc.manifests.json";
    }
}
