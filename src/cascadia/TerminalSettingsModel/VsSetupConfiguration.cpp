// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "VsSetupConfiguration.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;

VsSetupConfiguration::VsSetupInstance::VsSetupInstance(ComPtrSetupQuery pQuery, ComPtrSetupInstance pInstance) :
    query(pQuery), // Copy and AddRef the query object.
    inst(std::move(pInstance)), // Take ownership of the instance object.
    installDate(GetInstallDate()),
    version(GetInstallationVersion())
{
    ComPtrCatalogPropertyStore catalogProperties = GetCatalogPropertyStore();
    ComPtrCustomPropertyStore customProperties = GetCustomPropertyStore();
    ComPtrPropertyStore instanceProperties = GetInstancePropertyStore();

    const auto nickname = customProperties ? GetNickname(customProperties.get()) : std::wstring{};
    auto channelName = instanceProperties ? GetChannelName(instanceProperties.get()) : std::wstring{};

    if (channelName == L"Release")
    {
        channelName.clear();
    }

    isRelease = channelName.empty();

    if (catalogProperties)
    {
        profileNameSuffix.append(GetProductLineVersion(catalogProperties.get()));

        if (!nickname.empty())
        {
            profileNameSuffix.append(L" (");
            profileNameSuffix.append(nickname);
            profileNameSuffix.append(L")");
        }

        if (!channelName.empty())
        {
            profileNameSuffix.append(L" [");
            profileNameSuffix.append(channelName);
            profileNameSuffix.append(L"]");
        }
    }
    else
    {
        profileNameSuffix = GetVersion();
    }
}

std::vector<VsSetupConfiguration::VsSetupInstance> VsSetupConfiguration::QueryInstances()
{
    std::vector<VsSetupInstance> instances;

    // SetupConfiguration is only registered if Visual Studio is installed
    ComPtrSetupQuery pQuery{ wil::CoCreateInstanceNoThrow<SetupConfiguration, ISetupConfiguration2>() };
    if (pQuery == nullptr)
    {
        return instances;
    }

    // Enumerate all valid instances of Visual Studio
    wil::com_ptr<IEnumSetupInstances> e;
    THROW_IF_FAILED(pQuery->EnumInstances(&e));

    for (ComPtrSetupInstance rgpInstance; S_OK == THROW_IF_FAILED(e->Next(1, &rgpInstance, nullptr));)
    {
        instances.emplace_back(pQuery, std::move(rgpInstance));
    }

    if (instances.empty())
    {
        return instances;
    }

    // Sort instances based on version and install date from latest to oldest.
    const auto beg = instances.begin();
    const auto end = instances.end();
    std::sort(beg, end, [](const VsSetupInstance& a, const VsSetupInstance& b) {
        auto const aVersion = a.GetComparableVersion();
        auto const bVersion = b.GetComparableVersion();

        if (aVersion == bVersion)
        {
            return a.GetComparableInstallDate() > b.GetComparableInstallDate();
        }

        return aVersion > bVersion;
    });

    // The first instance is the most preferred one and the only one that isn't hidden by default.
    // This code tries to prefer any installed Release version of VS over Preview ones.
    if (!instances[0].IsRelease())
    {
        const auto it = std::find_if(beg + 1, end, [](const VsSetupInstance& a) { return a.IsRelease(); });
        if (it != end)
        {
            std::rotate(beg, it, end);
        }
    }

    return instances;
}

/// <summary>
/// Takes a relative path under a Visual Studio installation and returns the absolute path.
/// </summary>
std::wstring VsSetupConfiguration::ResolvePath(ISetupInstance* pInst, std::wstring_view relativePath)
{
    wil::unique_bstr bstrAbsolutePath;
    THROW_IF_FAILED(pInst->ResolvePath(relativePath.data(), &bstrAbsolutePath));
    return bstrAbsolutePath.get();
}

/// <summary>
/// Determines whether a Visual Studio installation version falls within a specified range.
/// The range is specified as a string, ex: "[15.0.0.0,)", "[15.0.0.0, 16.7.0.0)
/// </summary>
bool VsSetupConfiguration::InstallationVersionInRange(ISetupConfiguration2* pQuery, ISetupInstance* pInst, std::wstring_view range)
{
    const auto helper = wil::com_query<ISetupHelper>(pQuery);

    // VS versions in a string format such as "16.3.0.0" can be easily compared
    // by parsing them into 64-bit unsigned integers using the stable algorithm
    // provided by ParseVersion and ParseVersionRange

    unsigned long long minVersion{ 0 };
    unsigned long long maxVersion{ 0 };
    THROW_IF_FAILED(helper->ParseVersionRange(range.data(), &minVersion, &maxVersion));

    wil::unique_bstr bstrVersion;
    THROW_IF_FAILED(pInst->GetInstallationVersion(&bstrVersion));

    unsigned long long ullVersion{ 0 };
    THROW_IF_FAILED(helper->ParseVersion(bstrVersion.get(), &ullVersion));

    return ullVersion >= minVersion && ullVersion <= maxVersion;
}

std::wstring VsSetupConfiguration::GetInstallationVersion(ISetupInstance* pInst)
{
    wil::unique_bstr bstrInstallationVersion;
    THROW_IF_FAILED(pInst->GetInstallationVersion(&bstrInstallationVersion));
    return bstrInstallationVersion.get();
}

std::wstring VsSetupConfiguration::GetInstallationPath(ISetupInstance* pInst)
{
    wil::unique_bstr bstrInstallationPath;
    THROW_IF_FAILED(pInst->GetInstallationPath(&bstrInstallationPath));
    return bstrInstallationPath.get();
}

/// <summary>
/// The instance id is unique for each Visual Studio installation on a system.
/// The instance id is generated by the Visual Studio setup engine and varies from system to system.
/// </summary>
std::wstring VsSetupConfiguration::GetInstanceId(ISetupInstance* pInst)
{
    wil::unique_bstr bstrInstanceId;
    THROW_IF_FAILED(pInst->GetInstanceId(&bstrInstanceId));
    return bstrInstanceId.get();
}

unsigned long long VsSetupConfiguration::GetInstallDate(ISetupInstance* pInst)
{
    FILETIME ftInstallDate{ 0 };
    THROW_IF_FAILED(pInst->GetInstallDate(&ftInstallDate));
    return wil::filetime::to_int64(ftInstallDate);
}

std::wstring VsSetupConfiguration::GetStringProperty(ISetupPropertyStore* pProps, std::wstring_view name)
{
    if (pProps == nullptr)
    {
        return std::wstring{};
    }

    wil::unique_variant var;
    if (FAILED(pProps->GetValue(name.data(), &var)) || var.vt != VT_BSTR)
    {
        return std::wstring{};
    }

    return var.bstrVal;
}
