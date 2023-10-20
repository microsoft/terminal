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

#include "../inc/conint.h"

#include <initguid.h>

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

[[nodiscard]] static HRESULT _lookupCatalog(PCWSTR extensionName, std::vector<DelegationConfig::DelegationBase>& vec) noexcept
{
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
        DelegationConfig::PackageInfo extensionMetadata;

        ComPtr<IAppExtension> extension;
        RETURN_IF_FAILED(extensionList->GetAt(index, &extension));

        ComPtr<IPackage> extensionPackage;
        RETURN_IF_FAILED(extension->get_Package(&extensionPackage));

        ComPtr<IPackage2> extensionPackage2;
        RETURN_IF_FAILED(extensionPackage.As(&extensionPackage2));

        ComPtr<IPackageId> extensionPackageId;
        RETURN_IF_FAILED(extensionPackage->get_Id(&extensionPackageId));

        HString publisherId;
        RETURN_IF_FAILED(extensionPackageId->get_PublisherId(publisherId.GetAddressOf()));

        HString name;
        RETURN_IF_FAILED(extensionPackage2->get_DisplayName(name.GetAddressOf()));
        extensionMetadata.name = std::wstring{ name.GetRawBuffer(nullptr) };

        HString publisher;
        RETURN_IF_FAILED(extensionPackage2->get_PublisherDisplayName(publisher.GetAddressOf()));
        extensionMetadata.author = std::wstring{ publisher.GetRawBuffer(nullptr) };

        // Try to get the logo. Don't completely bail if we fail to get it. It's non-critical.
        ComPtr<IUriRuntimeClass> logoUri;
        LOG_IF_FAILED(extensionPackage2->get_Logo(logoUri.GetAddressOf()));

        // If we did manage to get one, extract the string and store in the structure
        if (logoUri)
        {
            HString logo;

            RETURN_IF_FAILED(logoUri->get_AbsoluteUri(logo.GetAddressOf()));
            extensionMetadata.logo = std::wstring{ logo.GetRawBuffer(nullptr) };
        }

        HString pfn;
        RETURN_IF_FAILED(extensionPackageId->get_FamilyName(pfn.GetAddressOf()));
        extensionMetadata.pfn = std::wstring{ pfn.GetRawBuffer(nullptr) };

        PackageVersion version;
        RETURN_IF_FAILED(extensionPackageId->get_Version(&version));
        extensionMetadata.version.major = version.Major;
        extensionMetadata.version.minor = version.Minor;
        extensionMetadata.version.build = version.Build;
        extensionMetadata.version.revision = version.Revision;

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

        // But this time that Inspectable is an IPropertyValue, which is a PROPVARIANT in a trench coat.
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

        vec.push_back({ iid, extensionMetadata });
    }

    return S_OK;
}

[[nodiscard]] HRESULT DelegationConfig::s_GetAvailablePackages(std::vector<DelegationPackage>& packages, DelegationPackage& def) noexcept
try
{
    auto coinit = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);

    packages.clear();
    packages.push_back({ DefaultDelegationPair });
    packages.push_back({ ConhostDelegationPair });

    // Get consoles and terminals.
    // If we fail to look up any, we should still have ONE come back to us as the hard-coded default console host.
    // The errors aren't really useful except for debugging, so log only.
    std::vector<DelegationBase> consoles;
    LOG_IF_FAILED(_lookupCatalog(DELEGATION_CONSOLE_EXTENSION_NAME, consoles));

    std::vector<DelegationBase> terminals;
    LOG_IF_FAILED(_lookupCatalog(DELEGATION_TERMINAL_EXTENSION_NAME, terminals));

    // TODO: I hate this algorithm (it's bad performance), but I couldn't
    // find an AppModel interface that would let me look up all the extensions
    // in one package.
    for (const auto& term : terminals)
    {
        for (const auto& con : consoles)
        {
            if (term.info.IsFromSamePackage(con.info))
            {
                DelegationPackage package;
                package.pair = { DelegationPairKind::Custom, con.clsid, term.clsid };
                package.info = term.info;
                packages.push_back(std::move(package));
                break;
            }
        }
    }

    // We should find at least one package.
    RETURN_HR_IF(E_FAIL, packages.empty());

    // Get the currently set default console/terminal.
    // Then, search through and find a package that matches.
    // If we find one, then return it.
    const auto delegationPair = s_GetDelegationPair();
    for (auto& pkg : packages)
    {
        if (pkg.pair == delegationPair)
        {
            def = pkg;
            return S_OK;
        }
    }

    // The default is DefaultDelegationPair ("Let Windows decide").
    def = packages.at(0);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT DelegationConfig::s_SetDefaultConsoleById(const IID& iid) noexcept
{
    return s_Set(DELEGATION_CONSOLE_KEY_NAME, iid);
}

[[nodiscard]] HRESULT DelegationConfig::s_SetDefaultTerminalById(const IID& iid) noexcept
{
    return s_Set(DELEGATION_TERMINAL_KEY_NAME, iid);
}

[[nodiscard]] HRESULT DelegationConfig::s_SetDefaultByPackage(const DelegationPackage& package) noexcept
{
    RETURN_IF_FAILED(s_SetDefaultConsoleById(package.pair.console));
    RETURN_IF_FAILED(s_SetDefaultTerminalById(package.pair.terminal));
    return S_OK;
}

[[nodiscard]] DelegationConfig::DelegationPair DelegationConfig::s_GetDelegationPair() noexcept
{
    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;
    wil::unique_hkey startupKey;
    if (FAILED_NTSTATUS_LOG(RegistrySerialization::s_OpenConsoleKey(&currentUserKey, &consoleKey)) ||
        FAILED_NTSTATUS_LOG(RegistrySerialization::s_OpenKey(consoleKey.get(), L"%%Startup", &startupKey)))
    {
        return DefaultDelegationPair;
    }

    static constexpr const wchar_t* keys[2]{ DELEGATION_CONSOLE_KEY_NAME, DELEGATION_TERMINAL_KEY_NAME };
    // values[0]/[1] will contain the delegation console/terminal
    // respectively if set to a valid value within the registry.
    IID values[2]{ CLSID_Default, CLSID_Default };

    for (size_t i = 0; i < 2; ++i)
    {
        // The GUID is stored as: {00000000-0000-0000-0000-000000000000}
        // = 38 characters + trailing null terminator.
        wchar_t buffer[39];
        DWORD bytesUsed = 0;
        const auto result = RegistrySerialization::s_QueryValue(startupKey.get(), keys[i], sizeof(buffer), REG_SZ, reinterpret_cast<BYTE*>(&buffer[0]), &bytesUsed);
        if (result != S_OK)
        {
            // Don't log the more common key-not-found error.
            if (result != NTSTATUS_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                LOG_NTSTATUS(result);
            }
            continue;
        }

        if (bytesUsed == sizeof(buffer))
        {
            // RegQueryValueExW docs:
            // If the data has the REG_SZ, REG_MULTI_SZ or REG_EXPAND_SZ type, the string may not have been stored with the
            // proper terminating null characters. Therefore, even if the function returns ERROR_SUCCESS, the application
            // should ensure that the string is properly terminated before using it; otherwise, it may overwrite a buffer.
            buffer[std::size(buffer) - 1] = 0;
            LOG_IF_FAILED(IIDFromString(&buffer[0], &values[i]));
        }
    }

    if (values[0] == CLSID_Default || values[1] == CLSID_Default)
    {
        return DefaultDelegationPair;
    }
    if (values[0] == CLSID_Conhost || values[1] == CLSID_Conhost)
    {
        return ConhostDelegationPair;
    }
    return { DelegationPairKind::Custom, values[0], values[1] };
}

[[nodiscard]] HRESULT DelegationConfig::s_Set(PCWSTR value, const CLSID clsid) noexcept
try
{
    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenConsoleKey(&currentUserKey, &consoleKey));

    // Create method for registry is a "create if not exists, otherwise open" function.
    wil::unique_hkey startupKey;
    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_CreateKey(consoleKey.get(), L"%%Startup", &startupKey));

    wil::unique_cotaskmem_string str;
    RETURN_IF_FAILED(StringFromCLSID(clsid, &str));

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_SetValue(startupKey.get(), value, REG_SZ, reinterpret_cast<BYTE*>(str.get()), gsl::narrow<DWORD>(wcslen(str.get()) * sizeof(wchar_t))));

    return S_OK;
}
CATCH_RETURN()
