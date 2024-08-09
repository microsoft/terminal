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

#include <winrt/Microsoft.Management.Deployment.h>

namespace winrt::TerminalApp::implementation
{
    struct WindowsPackageManagerFactory
    {
    public:
        static winrt::Microsoft::Management::Deployment::PackageManager CreatePackageManager()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::PackageManager>(PackageManagerGuid, CLSCTX_ALL);
        }

        static winrt::Microsoft::Management::Deployment::FindPackagesOptions CreateFindPackagesOptions()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::FindPackagesOptions>(FindPackageOptionsGuid, CLSCTX_ALL);
        }

        static winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions CreateCreateCompositePackageCatalogOptions()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions>(CreateCompositePackageCatalogOptionsGuid, CLSCTX_ALL);
        }

        static winrt::Microsoft::Management::Deployment::InstallOptions CreateInstallOptions()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::InstallOptions>(InstallOptionsGuid, CLSCTX_ALL);
        }

        static winrt::Microsoft::Management::Deployment::UninstallOptions CreateUninstallOptions()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::UninstallOptions>(UninstallOptionsGuid, CLSCTX_ALL);
        }

        static winrt::Microsoft::Management::Deployment::PackageMatchFilter CreatePackageMatchFilter()
        {
            return winrt::create_instance<winrt::Microsoft::Management::Deployment::PackageMatchFilter>(PackageMatchFilterGuid, CLSCTX_ALL);
        }

    private:
        static constexpr winrt::guid PackageManagerGuid{ 0xC53A4F16, 0x787E, 0x42A4, { 0xB3, 0x04, 0x29, 0xEF, 0xFB, 0x4B, 0xF5, 0x97 } };
        static constexpr winrt::guid FindPackageOptionsGuid{ 0x572DED96, 0x9C60, 0x4526, { 0x8F, 0x92, 0xEE, 0x7D, 0x91, 0xD3, 0x8C, 0x1A } };
        static constexpr winrt::guid CreateCompositePackageCatalogOptionsGuid{ 0x526534B8, 0x7E46, 0x47C8, { 0x84, 0x16, 0xB1, 0x68, 0x5C, 0x32, 0x7D, 0x37 } };
        static constexpr winrt::guid InstallOptionsGuid{ 0x1095F097, 0xEB96, 0x453B, { 0xB4, 0xE6, 0x16, 0x13, 0x63, 0x7F, 0x3B, 0x14 } };
        static constexpr winrt::guid UninstallOptionsGuid{ 0xE1D9A11E, 0x9F85, 0x4D87, { 0x9C, 0x17, 0x2B, 0x93, 0x14, 0x3A, 0xDB, 0x8D } };
        static constexpr winrt::guid PackageMatchFilterGuid{ 0xD02C9DAF, 0x99DC, 0x429C, { 0xB5, 0x03, 0x4E, 0x50, 0x4E, 0x4A, 0xB0, 0x00 } };
    };
}
