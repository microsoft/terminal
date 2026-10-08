// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalDsc/Resource/ResourceRegistry.h"
#include "../TerminalDsc/Resources/Settings/SettingsResource.h"

using namespace Microsoft::Terminal::Dsc;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace TerminalDscUnitTests
{
    class ResourceRegistryTests
    {
        TEST_CLASS(ResourceRegistryTests);

        TEST_METHOD(RegistrationCapturesCapabilities);
        TEST_METHOD(FindIsCaseInsensitive);
        TEST_METHOD(DuplicateRegistrationThrows);
        TEST_METHOD(SingleResourceDetection);
    };

    void ResourceRegistryTests::RegistrationCapturesCapabilities()
    {
        ResourceRegistry registry;
        registry.Add(std::make_unique<SettingsResource>());

        const auto registration{ registry.Find("Microsoft.WindowsTerminal/Settings") };
        VERIFY_IS_NOT_NULL(registration);
        VERIFY_IS_NOT_NULL(registration->get);
        VERIFY_IS_NOT_NULL(registration->set);
        VERIFY_IS_NOT_NULL(registration->exp);
        VERIFY_IS_NULL(registration->test);
        VERIFY_IS_NULL(registration->del);
    }

    void ResourceRegistryTests::FindIsCaseInsensitive()
    {
        ResourceRegistry registry;
        registry.Add(std::make_unique<SettingsResource>());

        VERIFY_IS_NOT_NULL(registry.Find("microsoft.windowsterminal/settings"));
        VERIFY_IS_NOT_NULL(registry.Find("MICROSOFT.WINDOWSTERMINAL/SETTINGS"));
        VERIFY_IS_NULL(registry.Find("Microsoft.WindowsTerminal/DoesNotExist"));
    }

    void ResourceRegistryTests::DuplicateRegistrationThrows()
    {
        ResourceRegistry registry;
        registry.Add(std::make_unique<SettingsResource>());

        VERIFY_THROWS_SPECIFIC(registry.Add(std::make_unique<SettingsResource>()), const std::invalid_argument, [](const auto&) { return true; });
    }

    void ResourceRegistryTests::SingleResourceDetection()
    {
        ResourceRegistry registry;
        VERIFY_IS_FALSE(registry.IsSingleResource());

        registry.Add(std::make_unique<SettingsResource>());
        VERIFY_IS_TRUE(registry.IsSingleResource());
        VERIFY_ARE_EQUAL(static_cast<size_t>(1), registry.Count());
    }
}
