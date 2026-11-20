// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalDsc/Resource/JsonUtils.h"
#include "../TerminalDsc/Resource/ManifestGenerator.h"
#include "../TerminalDsc/Resources/Settings/SettingsResource.h"

using namespace Microsoft::Terminal::Dsc;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalDscUnitTests
{
    namespace
    {
        // A second, minimal resource to exercise the multi-resource shapes.
        struct FakeResource final : IDscResource, IGettable
        {
            const ResourceMetadata& Metadata() const noexcept override
            {
                static const ResourceMetadata metadata{
                    .type = "Test/Fake",
                    .version = "1.0.0",
                    .description = "A fake resource for tests.",
                    .setReturn = SetReturn::None,
                };
                return metadata;
            }

            Json::Value Schema() const override
            {
                Json::Value schema{ Json::objectValue };
                schema["type"] = "object";
                return schema;
            }

            Json::Value Get(const std::optional<Json::Value>& /*instance*/) override
            {
                return Json::Value{ Json::objectValue };
            }
        };
    }

    class ManifestGeneratorTests
    {
        TEST_CLASS(ManifestGeneratorTests);

        TEST_METHOD(GeneratedManifestMatchesCheckedInFile);
        TEST_METHOD(MultiResourceDocumentAndFileNames);
    };

    void ManifestGeneratorTests::GeneratedManifestMatchesCheckedInFile()
    {
        static constexpr std::string_view expectedManifest{ R"({
            "$schema": "https://aka.ms/dsc/schemas/v3/bundled/resource/manifest.json",
            "description": "Manage Windows Terminal global settings.",
            "exitCodes": {
                "0": "Success",
                "1": "Error",
                "2": "Invalid desired state input",
                "3": "Failed to load or save the Windows Terminal settings file"
            },
            "export": {
                "args": ["export", { "jsonInputArg": "--input", "mandatory": false }],
                "executable": "TerminalDsc"
            },
            "get": {
                "args": ["get", { "jsonInputArg": "--input", "mandatory": false }],
                "executable": "TerminalDsc"
            },
            "schema": {
                "embedded": {
                    "$schema": "https://json-schema.org/draft/2020-12/schema",
                    "additionalProperties": false,
                    "properties": {
                        "copyOnSelect": {
                            "description": "When true, a selection is immediately copied to the clipboard upon creation.",
                            "type": "boolean"
                        },
                        "initialCols": {
                            "description": "The number of columns displayed in the window upon first load.",
                            "maximum": 999,
                            "minimum": 1,
                            "type": "integer"
                        },
                        "theme": {
                            "description": "The theme of the application. Either the name of a built-in ('dark', 'light', 'system') or custom theme, or a pair of theme names applied when the OS is in dark or light mode.",
                            "oneOf": [
                                { "type": "string" },
                                {
                                    "additionalProperties": false,
                                    "properties": {
                                        "dark": { "type": "string" },
                                        "light": { "type": "string" }
                                    },
                                    "required": ["dark", "light"],
                                    "type": "object"
                                }
                            ]
                        }
                    },
                    "type": "object"
                }
            },
            "set": {
                "args": ["set", { "jsonInputArg": "--input", "mandatory": true }],
                "executable": "TerminalDsc",
                "return": "state"
            },
            "tags": ["Windows", "WindowsTerminal", "Terminal"],
            "type": "Microsoft.WindowsTerminal/Settings",
            "version": "0.1.0"
        })" };

        ResourceRegistry registry;
        registry.Add(std::make_unique<SettingsResource>());

        const auto document{ ManifestGenerator::BuildManifestDocument(registry, "TerminalDsc") };

        // Compare serialized forms: jsoncpp stores object members sorted, so
        // this is insensitive to authoring order but pins names and values.
        VERIFY_ARE_EQUAL(SerializeJson(ParseJson(expectedManifest)), SerializeJson(document));

        VERIFY_ARE_EQUAL(std::string{ "microsoft.windowsterminal.settings.dsc.resource.json" },
                         ManifestGenerator::ManifestFileName(registry, "TerminalDsc"));
    }

    void ManifestGeneratorTests::MultiResourceDocumentAndFileNames()
    {
        ResourceRegistry registry;
        registry.Add(std::make_unique<SettingsResource>());
        registry.Add(std::make_unique<FakeResource>());

        const auto document{ ManifestGenerator::BuildManifestDocument(registry, "TerminalDsc") };

        // With two or more resources the document becomes a manifest list and
        // every operation's args carry the --resource selector.
        VERIFY_IS_TRUE(document.isMember("resources"));
        VERIFY_ARE_EQUAL(2u, document["resources"].size());

        const auto& first{ document["resources"][0] };
        const auto& args{ first["get"]["args"] };
        VERIFY_ARE_EQUAL(std::string{ "get" }, args[0].asString());
        VERIFY_ARE_EQUAL(std::string{ "--resource" }, args[1].asString());
        VERIFY_ARE_EQUAL(std::string{ "Microsoft.WindowsTerminal/Settings" }, args[2].asString());

        // The fake resource is get-only: no set/export sections.
        const auto& second{ document["resources"][1] };
        VERIFY_IS_TRUE(second.isMember("get"));
        VERIFY_IS_FALSE(second.isMember("set"));
        VERIFY_IS_FALSE(second.isMember("export"));
        VERIFY_IS_FALSE(second.isMember("tags"));
        VERIFY_IS_FALSE(second.isMember("exitCodes"));

        VERIFY_ARE_EQUAL(std::string{ "TerminalDsc.dsc.manifests.json" },
                         ManifestGenerator::ManifestFileName(registry, "TerminalDsc"));
    }
}
