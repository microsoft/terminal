// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DelegationConfig.hpp"

#include "RegistrySerialization.hpp"

#include <wil/resource.h>
#include <wil/winrt.h>
#include <windows.foundation.collections.h>
#include <Windows.ApplicationModel.h>
#include <Windows.ApplicationModel.AppExtensions.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::ApplicationModel::AppExtensions;

#pragma hdrstop

#define DELEGATION_CONSOLE_KEY_NAME L"DelegationConsole"
#define DELEGATION_TERMINAL_KEY_NAME L"DelegationTerminal"

#define DELEGATION_CONSOLE_EXTENSION_NAME L"com.microsoft.windows.console.host"
#define DELEGATION_TERMINAL_EXTENSION_NAME L"com.microsoft.windows.terminal.host"

template<typename T, typename std::enable_if<std::is_base_of<DelegationConfig::DelegationBase, T>::value>::type* = nullptr>
HRESULT _lookupCatalog(PCWSTR extensionName, std::vector<T>& vec) noexcept
{
    vec.clear();

    auto coinit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    ComPtr<IAppExtensionCatalogStatics> catalogStatics;
    RETURN_IF_FAILED(Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_ApplicationModel_AppExtensions_AppExtensionCatalog).Get(), &catalogStatics));

    ComPtr<IAppExtensionCatalog> catalog;
    RETURN_IF_FAILED(catalogStatics->Open(HStringReference(extensionName).Get(), &catalog));

    ComPtr<IAsyncOperation<IVectorView<AppExtension*>*>> findOperation;
    RETURN_IF_FAILED(catalog->FindAllAsync(&findOperation));

    ComPtr<IVectorView<AppExtension*>> extensionList;
    RETURN_IF_FAILED(wil::wait_for_completion_nothrow(findOperation.Get(), &extensionList));

    UINT extensionCount;
    RETURN_IF_FAILED(extensionList->get_Size(&extensionCount));
    for (UINT index = 0; index < extensionCount; index++)
    {
        T extensionMetadata;

        ComPtr<IAppExtension> extension;
        RETURN_IF_FAILED(extensionList->GetAt(index, &extension));

        ComPtr<IPackage> extensionPackage;
        RETURN_IF_FAILED(extension->get_Package(&extensionPackage));

        ComPtr<IPackageId> extensionPackageId;
        RETURN_IF_FAILED(extensionPackage->get_Id(&extensionPackageId));

        HString publisherId;
        RETURN_IF_FAILED(extensionPackageId->get_PublisherId(publisherId.GetAddressOf()));

        // PackageId.Name
        HString name;
        RETURN_IF_FAILED(extensionPackageId->get_Name(name.GetAddressOf()));

        extensionMetadata.name = std::wstring{ name.GetRawBuffer(nullptr) };

        // PackageId.Version
        HString publisher;
        RETURN_IF_FAILED(extensionPackageId->get_Publisher(publisher.GetAddressOf()));

        extensionMetadata.author = std::wstring{ publisher.GetRawBuffer(nullptr) };

        // Fetch the custom properties XML out of the extension information
        ComPtr<IAsyncOperation<IPropertySet*>> propertiesOperation;
        RETURN_IF_FAILED(extension->GetExtensionPropertiesAsync(&propertiesOperation));

        // Wait for async to complete and return the property set.
        ComPtr<IPropertySet> properties;
        RETURN_IF_FAILED(wil::wait_for_completion_nothrow(propertiesOperation.Get(), &properties));

        // We can't do anything on a set, but it must also be convertible to this type of map per the Windows.Foundation specs for this
        ComPtr<IMap<HSTRING, IInspectable*>> map;
        RETURN_IF_FAILED(properties.As(&map));

        // Looking it up is going to get us an inspectable
        ComPtr<IInspectable> inspectable;
        RETURN_IF_FAILED(map->Lookup(HStringReference(L"Clsid").Get(), &inspectable));

        // Unfortunately that inspectable is another set because we're dealing with XML data payload that we put in the manifest.
        ComPtr<IPropertySet> anotherSet;
        RETURN_IF_FAILED(inspectable.As(&anotherSet));

        // And we can't look at sets directly, so move it to map.
        ComPtr<IMap<HSTRING, IInspectable*>> anotherMap;
        RETURN_IF_FAILED(anotherSet.As(&anotherMap));

        // Use the magic value of #text to get the body between the XML tags. And of course it's an obtuse Inspectable again.
        ComPtr<IInspectable> anotherInspectable;
        RETURN_IF_FAILED(anotherMap->Lookup(HStringReference(L"#text").Get(), &anotherInspectable));

        // But this time that Inspectable is an IPropertyValue, which is a PROPVARIANT in a trenchcoat.
        ComPtr<IPropertyValue> propValue;
        RETURN_IF_FAILED(anotherInspectable.As(&propValue));

        // Check the type of the variant
        PropertyType propType;
        RETURN_IF_FAILED(propValue->get_Type(&propType));

        // If we're not a string, bail because I don't know what's going on.
        if (propType != PropertyType::PropertyType_String)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        // Get that string out.
        HString value;
        RETURN_IF_FAILED(propValue->GetString(value.GetAddressOf()));

        // Holy cow. It should be a GUID. Try to parse it.
        IID iid;
        RETURN_IF_FAILED(IIDFromString(value.GetRawBuffer(nullptr), &iid));

        extensionMetadata.clsid = iid;

        vec.emplace_back(std::move(extensionMetadata));
    }

    return S_OK;
}

[[nodiscard]] HRESULT DelegationConfig::s_GetAvailableConsoles(std::vector<DelegationConsole>& consoles) noexcept
try
{
    return _lookupCatalog(DELEGATION_CONSOLE_EXTENSION_NAME, consoles);
}
CATCH_RETURN()

[[nodiscard]] HRESULT DelegationConfig::s_GetAvailableTerminals(std::vector<DelegationTerminal>& terminals) noexcept
try
{
    return _lookupCatalog(DELEGATION_TERMINAL_EXTENSION_NAME, terminals);
}
CATCH_RETURN()

[[nodiscard]] HRESULT DelegationConfig::s_SetConsole(const DelegationConsole& console) noexcept
{
    return s_Set(DELEGATION_CONSOLE_KEY_NAME, console.clsid);
}

[[nodiscard]] HRESULT DelegationConfig::s_SetTerminal(const DelegationTerminal& terminal) noexcept
{
    return s_Set(DELEGATION_TERMINAL_KEY_NAME, terminal.clsid);
}

[[nodiscard]] HRESULT DelegationConfig::s_GetConsole(IID& iid) noexcept
{
    return s_Get(DELEGATION_CONSOLE_KEY_NAME, iid);
}

[[nodiscard]] HRESULT DelegationConfig::s_GetTerminal(IID& iid) noexcept
{
    return s_Get(DELEGATION_TERMINAL_KEY_NAME, iid);
}

[[nodiscard]] HRESULT DelegationConfig::s_Get(PCWSTR value, IID& iid) noexcept
{
    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenConsoleKey(&currentUserKey, &consoleKey));

    wil::unique_hkey startupKey;
    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenKey(consoleKey.get(), L"%%Startup", &startupKey));

    DWORD bytesNeeded = 0;
    NTSTATUS result = RegistrySerialization::s_QueryValue(startupKey.get(),
                                                          value,
                                                          0,
                                                          REG_SZ,
                                                          nullptr,
                                                          &bytesNeeded);

    if (NTSTATUS_FROM_WIN32(ERROR_SUCCESS) != result)
    {
        RETURN_NTSTATUS(result);
    }

    auto buffer = std::make_unique<wchar_t[]>(bytesNeeded / sizeof(wchar_t));

    DWORD bytesUsed = 0;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_QueryValue(startupKey.get(),
                                                                  value,
                                                                  bytesNeeded,
                                                                  REG_SZ,
                                                                  reinterpret_cast<BYTE*>(buffer.get()),
                                                                  &bytesUsed));

    RETURN_IF_FAILED(IIDFromString(buffer.get(), &iid));

    return S_OK;
}

[[nodiscard]] HRESULT DelegationConfig::s_Set(PCWSTR value, const CLSID clsid) noexcept
try
{
    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenConsoleKey(&currentUserKey, &consoleKey));

    wil::unique_hkey startupKey;
    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenKey(consoleKey.get(), L"%%Startup", &startupKey));

    wil::unique_cotaskmem_string str;
    RETURN_IF_FAILED(StringFromCLSID(clsid, &str));

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_SetValue(startupKey.get(), value, REG_SZ, reinterpret_cast<BYTE*>(str.get()), gsl::narrow<DWORD>(wcslen(str.get() + 1) * sizeof(wchar_t))));

    return S_OK;
}
CATCH_RETURN()
