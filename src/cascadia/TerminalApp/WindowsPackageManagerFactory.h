// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - WindowsPackageManagerFactory.h
//
// Abstract:
// - This factory is designed to create production-level instances of WinGet objects.
//   Elevated sessions require manual activation of WinGet objects and are not currently supported,
//   while non-elevated sessions can use the standard WinRT activation system.
// Author:
// - Carlos Zamora (carlos-zamora) 23-Jul-2024

#pragma once

#include "../../types/inc/utils.hpp"
#include <combaseapi.h>
#include <wil/result_macros.h>
#include <wil/win32_helpers.h>
#include <type_traits>

#include <winrt/Microsoft.Management.Deployment.h>

namespace winrt::TerminalApp::implementation
{
    class WindowsPackageManagerFactory
    {
    public:
        static WindowsPackageManagerFactory& Instance()
        {
            static WindowsPackageManagerFactory instance;
            return instance;
        }

        // Delete copy constructor and assignment operator
        WindowsPackageManagerFactory(const WindowsPackageManagerFactory&) = delete;
        WindowsPackageManagerFactory& operator=(const WindowsPackageManagerFactory&) = delete;

        winrt::Microsoft::Management::Deployment::PackageManager CreatePackageManager()
        {
            static winrt::guid clsid{ "C53A4F16-787E-42A4-B304-29EFFB4BF597" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::PackageManager>(clsid, CLSCTX_ALL);
        }

        winrt::Microsoft::Management::Deployment::FindPackagesOptions CreateFindPackagesOptions()
        {
            static winrt::guid clsid{ "572DED96-9C60-4526-8F92-EE7D91D38C1A" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::FindPackagesOptions>(clsid, CLSCTX_ALL);
        }

        winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions CreateCreateCompositePackageCatalogOptions()
        {
            static winrt::guid clsid{ "526534B8-7E46-47C8-8416-B1685C327D37" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions>(clsid, CLSCTX_ALL);
        }

        winrt::Microsoft::Management::Deployment::InstallOptions CreateInstallOptions()
        {
            static winrt::guid clsid{ "1095F097-EB96-453B-B4E6-1613637F3B14" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::InstallOptions>(clsid, CLSCTX_ALL);
        }

        winrt::Microsoft::Management::Deployment::UninstallOptions CreateUninstallOptions()
        {
            static winrt::guid clsid{ "E1D9A11E-9F85-4D87-9C17-2B93143ADB8D" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::UninstallOptions>(clsid, CLSCTX_ALL);
        }

        winrt::Microsoft::Management::Deployment::PackageMatchFilter CreatePackageMatchFilter()
        {
            static winrt::guid clsid{ "D02C9DAF-99DC-429C-B503-4E504E4AB000" };
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::PackageMatchFilter>(clsid, CLSCTX_ALL);
        }

    private:
        WindowsPackageManagerFactory() = default;
    };
}
