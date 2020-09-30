// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "VsSetupConfiguration.h"

std::vector<TerminalApp::VsSetupConfiguration::VsSetupInstance> TerminalApp::VsSetupConfiguration::QueryInstances()
{
    std::vector<VsSetupConfiguration::VsSetupInstance> instances;

    ComPtrSetupQuery pQuery{ wil::CoCreateInstanceNoThrow<SetupConfiguration, ISetupConfiguration2>() };
    if (pQuery == nullptr)
    {
        return instances;
    }

    wil::com_ptr<IEnumSetupInstances> e;
    THROW_IF_FAILED(pQuery->EnumInstances(&e));

    ISetupInstance* pInstances[1] = {};
    auto result = e->Next(1, pInstances, NULL);
    while (S_OK == result)
    {
        auto i = wil::com_ptr<ISetupInstance>{ pInstances[0] };
        instances.emplace_back(VsSetupConfiguration::VsSetupInstance{ pQuery, i.query<ISetupInstance2>() });

        result = e->Next(1, pInstances, NULL);
    }

    THROW_IF_FAILED(result);

    return instances;
}

std::wstring TerminalApp::VsSetupConfiguration::ResolvePath(ComPtrSetupInstance pInst, std::wstring_view relativePath)
{
    wil::unique_bstr bstrAbsolutePath;
    THROW_IF_FAILED(pInst->ResolvePath(relativePath.data(), &bstrAbsolutePath));
    return bstrAbsolutePath.get();
}

bool TerminalApp::VsSetupConfiguration::InstallationVersionInRange(ComPtrSetupQuery pQuery, ComPtrSetupInstance pInst, std::wstring_view range)
{
    auto helper = pQuery.query<ISetupHelper>();

    ULONGLONG minVersion;
    ULONGLONG maxVersion;
    THROW_IF_FAILED(helper->ParseVersionRange(range.data(), &minVersion, &maxVersion));

    wil::unique_bstr bstrVersion;
    THROW_IF_FAILED(pInst->GetInstallationVersion(&bstrVersion));

    ULONGLONG ullVersion;
    THROW_IF_FAILED(helper->ParseVersion(bstrVersion.get(), &ullVersion));

    return ullVersion >= minVersion && ullVersion <= maxVersion;
}

std::wstring TerminalApp::VsSetupConfiguration::GetInstallationVersion(ComPtrSetupInstance pInst)
{
    wil::unique_bstr bstrInstallationVersion;
    THROW_IF_FAILED(pInst->GetInstallationVersion(&bstrInstallationVersion));
    return bstrInstallationVersion.get();
}

std::wstring TerminalApp::VsSetupConfiguration::GetInstallationPath(ComPtrSetupInstance pInst)
{
    wil::unique_bstr bstrInstallationPath;
    THROW_IF_FAILED(pInst->GetInstallationPath(&bstrInstallationPath));
    return bstrInstallationPath.get();
}

std::wstring TerminalApp::VsSetupConfiguration::GetInstanceId(ComPtrSetupInstance pInst)
{
    wil::unique_bstr bstrInstanceId;
    THROW_IF_FAILED(pInst->GetInstanceId(&bstrInstanceId));
    return bstrInstanceId.get();
}

std::wstring TerminalApp::VsSetupConfiguration::GetStringProperty(ComPtrPropertyStore pProps, std::wstring_view name)
{
    wil::unique_variant var;
    THROW_IF_FAILED(pProps->GetValue(name.data(), &var));
    return var.bstrVal;
}
