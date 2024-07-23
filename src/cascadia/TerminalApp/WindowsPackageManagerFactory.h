// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - WindowsPackageManagerFactory.h
//
// Abstract:
// - These set of factories are designed to create production-level instances of WinGet objects.
//   Elevated sessions require manual activation of WinGet objects,
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
            return CreateInstance<winrt::Microsoft::Management::Deployment::PackageManager>();
        }

        winrt::Microsoft::Management::Deployment::FindPackagesOptions CreateFindPackagesOptions()
        {
            return CreateInstance<winrt::Microsoft::Management::Deployment::FindPackagesOptions>();
        }

        winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions CreateCreateCompositePackageCatalogOptions()
        {
            return CreateInstance<winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions>();
        }

        winrt::Microsoft::Management::Deployment::InstallOptions CreateInstallOptions()
        {
            return CreateInstance<winrt::Microsoft::Management::Deployment::InstallOptions>();
        }

        winrt::Microsoft::Management::Deployment::UninstallOptions CreateUninstallOptions()
        {
            return CreateInstance<winrt::Microsoft::Management::Deployment::UninstallOptions>();
        }

        winrt::Microsoft::Management::Deployment::PackageMatchFilter CreatePackageMatchFilter()
        {
            return CreateInstance<winrt::Microsoft::Management::Deployment::PackageMatchFilter>();
        }

    private:
        wil::unique_hmodule _winrtactModule;

        WindowsPackageManagerFactory()
        {
            if (::Microsoft::Console::Utils::IsRunningElevated() || true)
            {
                _winrtactModule.reset(LoadLibraryExW(L"winrtact.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
            }
        }

        template<typename T>
        T CreateInstance(const guid& clsid, const guid& iid)
        {
            if (::Microsoft::Console::Utils::IsRunningElevated() || true)
            {
                winrt::com_ptr<::IUnknown> result{};
                try
                {
                    extern HRESULT WinGetServerManualActivation_CreateInstance(REFCLSID rclsid, REFIID riid, UINT32 flags, void** out);

                    auto createFn = reinterpret_cast<HRESULT (*)(REFCLSID, REFIID, UINT32, void**)>(GetProcAddress(_winrtactModule.get(), "WinGetServerManualActivation_CreateInstance"));
                    THROW_LAST_ERROR_IF(!createFn);
                    THROW_IF_FAILED(createFn(clsid, iid, 0, result.put_void()));
                    return result.as<T>();
                }
                catch (...)
                {
                }
            }
            return winrt::create_instance<T>(clsid, CLSCTX_ALL);
        }

        template<typename T>
        T CreateInstance()
        {
            winrt::guid clsid, iid;
            if (std::is_same<T, winrt::Microsoft::Management::Deployment::PackageManager>::value)
            {
                clsid = winrt::guid{ "C53A4F16-787E-42A4-B304-29EFFB4BF597" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::IPackageManager>();
            }
            else if (std::is_same<T, winrt::Microsoft::Management::Deployment::FindPackagesOptions>::value)
            {
                clsid = winrt::guid{ "572DED96-9C60-4526-8F92-EE7D91D38C1A" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::IFindPackagesOptions>();
            }
            else if (std::is_same<T, winrt::Microsoft::Management::Deployment::CreateCompositePackageCatalogOptions>::value)
            {
                clsid = winrt::guid{ "526534B8-7E46-47C8-8416-B1685C327D37" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::ICreateCompositePackageCatalogOptions>();
            }
            else if (std::is_same<T, winrt::Microsoft::Management::Deployment::InstallOptions>::value)
            {
                clsid = winrt::guid{ "1095F097-EB96-453B-B4E6-1613637F3B14" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::IInstallOptions>();
            }
            else if (std::is_same<T, winrt::Microsoft::Management::Deployment::UninstallOptions>::value)
            {
                clsid = winrt::guid{ "E1D9A11E-9F85-4D87-9C17-2B93143ADB8D" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::IUninstallOptions>();
            }
            else if (std::is_same<T, winrt::Microsoft::Management::Deployment::PackageMatchFilter>::value)
            {
                clsid = winrt::guid{ "D02C9DAF-99DC-429C-B503-4E504E4AB000" };
                iid = winrt::guid_of<winrt::Microsoft::Management::Deployment::IPackageMatchFilter>();
            }

            return CreateInstance<T>(clsid, iid);
        }
    };
}
