// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Resource/CommandBuilder.h"
#include "Resource/ResourceRegistry.h"
#include "Resources/Settings/SettingsResource.h"

using namespace Microsoft::Terminal::Dsc;

int wmain(int argc, wchar_t* argv[])
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    // Register every Microsoft DSC resource this executable serves. To add a
    // resource: implement IDscResource (+ capability interfaces) under
    // Resources/<Name>/ and add it here
    ResourceRegistry registry;
    registry.Add(std::make_unique<SettingsResource>());

    CommandBuilder command{ std::move(registry) };

    std::vector<std::wstring_view> args;
    args.reserve(argc > 1 ? gsl::narrow_cast<size_t>(argc - 1) : 0);
    for (auto i = 1; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }

    return command.Run(args, std::cout);
}
