// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandBuilder.h"
#include "JsonUtils.h"
#include "Logger.h"
#include "ManifestGenerator.h"
#include "ResourceExecutor.h"

#include <fstream>
#include <wil/win32_helpers.h>

using namespace Microsoft::Terminal::Dsc;

namespace
{
    constexpr std::string_view usage{ "usage: TerminalDsc.exe <get|set|test|delete|export|schema|manifest> [--resource <type>] [--input <json>] [--what-if] [--save]" };

    struct ParsedArgs
    {
        std::string operation;
        std::optional<std::string> resourceType;
        std::optional<std::string> input;
        bool whatIf = false;
        bool save = false;
    };

    std::optional<ParsedArgs> parseArgs(const std::vector<std::wstring_view>& args)
    {
        if (args.empty())
        {
            Logger::WriteError(usage);
            return std::nullopt;
        }

        ParsedArgs parsed;
        parsed.operation = til::u16u8(args[0]);

        for (size_t i = 1; i < args.size(); ++i)
        {
            const auto& arg{ args[i] };
            if ((arg == L"--resource" || arg == L"-r") && i + 1 < args.size())
            {
                parsed.resourceType = til::u16u8(args[++i]);
            }
            else if ((arg == L"--input" || arg == L"-i") && i + 1 < args.size())
            {
                parsed.input = til::u16u8(args[++i]);
            }
            else if (arg == L"--what-if" || arg == L"-w")
            {
                parsed.whatIf = true;
            }
            else if (arg == L"--save")
            {
                parsed.save = true;
            }
            else
            {
                Logger::WriteError(fmt::format(FMT_COMPILE("unexpected argument '{}'; {}"), til::u16u8(arg), usage));
                return std::nullopt;
            }
        }
        return parsed;
    }

    // The stem the generated manifests reference as "executable"; PATH lookup
    // appends .exe on Windows.
    std::string executableStem()
    {
        const std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        return module.stem().string();
    }

    int runManifest(const ResourceRegistry& registry, const ParsedArgs& parsed, std::ostream& output)
    {
        const auto stem{ executableStem() };

        // With several resources, --resource narrows the output to one manifest.
        Json::Value document;
        if (parsed.resourceType && !registry.IsSingleResource())
        {
            const auto registration{ registry.Find(*parsed.resourceType) };
            if (!registration)
            {
                Logger::WriteError(fmt::format(FMT_COMPILE("unknown resource '{}'"), *parsed.resourceType));
                return ExitError;
            }
            document = ManifestGenerator::BuildResourceManifest(*registration, false, stem);
        }
        else
        {
            document = ManifestGenerator::BuildManifestDocument(registry, stem);
        }

        if (parsed.save)
        {
            // Write next to the executable, where Microsoft DSC discovers it.
            const std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            const auto path{ module.parent_path() / ManifestGenerator::ManifestFileName(registry, stem) };

            Json::StreamWriterBuilder builder;
            builder.settings_["indentation"] = "    ";
            builder.settings_["commentStyle"] = "None";

            std::ofstream file{ path, std::ios::binary };
            if (!file)
            {
                throw DscResourceError{ fmt::format(FMT_COMPILE("failed to write manifest to '{}'"), path.string()) };
            }
            file << Json::writeString(builder, document) << '\n';
            Logger::WriteInfo(fmt::format(FMT_COMPILE("manifest saved to '{}'"), path.string()));
        }
        else
        {
            output << SerializeJson(document) << '\n';
        }
        return ExitSuccess;
    }
}

namespace Microsoft::Terminal::Dsc
{
    int CommandBuilder::Run(const std::vector<std::wstring_view>& args, std::ostream& output)
    {
        const auto parsed{ parseArgs(args) };
        if (!parsed)
        {
            return ExitError;
        }

        static constexpr std::array knownOperations{ "get", "set", "test", "delete", "export", "schema", "manifest" };
        if (std::ranges::find(knownOperations, parsed->operation) == knownOperations.end())
        {
            Logger::WriteError(fmt::format(FMT_COMPILE("unknown operation '{}'; {}"), parsed->operation, usage));
            return ExitError;
        }

        try
        {
            if (parsed->operation == "manifest")
            {
                return runManifest(_registry, *parsed, output);
            }

            // Resolve the target resource. With a single registration the
            // --resource selector is implicit but, when given, must match.
            const ResourceRegistration* registration{ nullptr };
            if (parsed->resourceType)
            {
                registration = _registry.Find(*parsed->resourceType);
                if (!registration)
                {
                    Logger::WriteError(fmt::format(FMT_COMPILE("unknown resource '{}'"), *parsed->resourceType));
                    return ExitError;
                }
            }
            else if (_registry.IsSingleResource())
            {
                registration = &_registry.All().front();
            }
            else
            {
                Logger::WriteError(fmt::format(FMT_COMPILE("--resource is required; {}"), usage));
                return ExitError;
            }

            if (parsed->whatIf)
            {
                // No registered resource implements what-if yet; DSC
                // synthesizes it from test/get for those that don't.
                Logger::WriteError(fmt::format(FMT_COMPILE("resource '{}' does not support --what-if"), registration->Metadata().type));
                return ExitError;
            }

            const auto notSupported = [&](std::string_view operation) {
                Logger::WriteError(fmt::format(FMT_COMPILE("resource '{}' does not support the '{}' operation"), registration->Metadata().type, operation));
                return ExitError;
            };

            if (parsed->operation == "get")
            {
                if (!registration->get)
                {
                    return notSupported("get");
                }
                ResourceExecutor::ExecuteGet(*registration, parsed->input, output);
            }
            else if (parsed->operation == "set")
            {
                if (!registration->set)
                {
                    return notSupported("set");
                }
                ResourceExecutor::ExecuteSet(*registration, parsed->input, output);
            }
            else if (parsed->operation == "test")
            {
                if (!registration->test)
                {
                    return notSupported("test");
                }
                ResourceExecutor::ExecuteTest(*registration, parsed->input, output);
            }
            else if (parsed->operation == "delete")
            {
                if (!registration->del)
                {
                    return notSupported("delete");
                }
                ResourceExecutor::ExecuteDelete(*registration, parsed->input, output);
            }
            else if (parsed->operation == "export")
            {
                if (!registration->exp)
                {
                    return notSupported("export");
                }
                ResourceExecutor::ExecuteExport(*registration, parsed->input, output);
            }
            else if (parsed->operation == "schema")
            {
                ResourceExecutor::ExecuteSchema(*registration, output);
            }
        }
        catch (const DscInputError& e)
        {
            Logger::WriteError(e.what());
            return ExitInvalidInput;
        }
        catch (const DscResourceError& e)
        {
            Logger::WriteError(e.what());
            return ExitResourceError;
        }
        catch (const winrt::hresult_error& e)
        {
            Logger::WriteError(til::u16u8(e.message()));
            return ExitResourceError;
        }
        catch (const std::exception& e)
        {
            Logger::WriteError(e.what());
            return ExitError;
        }

        return ExitSuccess;
    }
}
