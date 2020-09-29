/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsSetupConfiguration

Abstract:
- Encapsulates the Visual Studio Setup Configuration COM APIs

Author(s):
- Charles Willis - October 2020

--*/

#pragma once

#include <wil/resource.h>
#include "Setup.Configuration.h"

namespace TerminalApp
{
    class VsSetupConfiguration
    {
    private:
        typedef wil::com_ptr<ISetupConfiguration2> ComPtrSetupQuery;
        typedef wil::com_ptr<ISetupHelper> ComPtrSetupHelper;
        typedef wil::com_ptr<ISetupInstance> ComPtrSetupInstance;
        typedef wil::com_ptr<ISetupPropertyStore> ComPtrPropertyStore;
        typedef wil::com_ptr<ISetupPackageReference> ComPtrPackageReference;
        typedef wil::com_ptr<ISetupInstanceCatalog> ComPtrInstanceCatalog;

    public:
        struct VsSetupInstance
        {
            inline const static std::wstring_view kVersion16_3{ L"16.3.0.0" };

            inline std::wstring ResolvePath(std::wstring_view relativePath) const
            {
                return VsSetupConfiguration::ResolvePath(mp_inst, relativePath);
            }

            inline std::wstring GetDevShellModulePath() const
            {
                ULONGLONG ullInstanceVersion;
                THROW_IF_FAILED(mp_helper->ParseVersion(GetVersion().data(), &ullInstanceVersion));

                ULONGLONG ullVersion16_3;
                THROW_IF_FAILED(mp_helper->ParseVersion(kVersion16_3.data(), &ullVersion16_3));

                if (ullInstanceVersion >= ullVersion16_3)
                    return ResolvePath(L"Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll");

                return ResolvePath(L"Common7\\Tools\\vsdevshell\\Microsoft.VisualStudio.DevShell.dll");
            }

            inline std::wstring GetDevCmdScriptPath() const
            {
                return ResolvePath(L"Common7\\Tools\\VsDevCmd.bat");
            }

            inline bool VersionInRange(std::wstring_view range) const
            {
                return VsSetupConfiguration::InstallationVersionInRange(mp_query, mp_inst, range);
            }

            inline std::wstring GetVersion() const
            {
                return VsSetupConfiguration::GetInstallationVersion(mp_inst);
            }

            inline std::wstring GetInstallationPath() const
            {
                return VsSetupConfiguration::GetInstallationPath(mp_inst);
            }

            inline std::wstring GetInstanceId() const
            {
                return VsSetupConfiguration::GetInstanceId(mp_inst);
            }

            inline std::wstring GetChannelId() const
            {
                ComPtrPropertyStore properties = mp_inst.query<ISetupPropertyStore>();
                return VsSetupConfiguration::GetStringProperty(properties, L"channelId");
            }

            inline std::wstring GetChannelName() const
            {
                std::wstring channelName;

                // channelId is in the format  <ProductName>.<MajorVersion>.<ChannelName>
                std::wstring channelId{ GetChannelId() };
                size_t pos = channelId.rfind(L".");
                if (pos != std::wstring::npos)
                {
                    channelName.append(channelId.substr(pos + 1));
                }

                return channelName;
            }

            inline std::wstring GetProductLineVersion() const
            {
                ComPtrPropertyStore properties;
                ComPtrInstanceCatalog instanceCatalog = mp_inst.query<ISetupInstanceCatalog>();
                THROW_IF_FAILED(instanceCatalog->GetCatalogInfo(&properties));
                return VsSetupConfiguration::GetStringProperty(properties, L"productLineVersion");
            }

            inline void DebugOutputProperties() const
            {
#ifdef _DEBUG
                wil::com_ptr<ISetupInstance2> instance2 = mp_inst.query<ISetupInstance2>();
                ComPtrPropertyStore properties;
                THROW_IF_FAILED(instance2->GetProperties(&properties));
                VsSetupConfiguration::DebugOutputProperties(L"Instance2::GetProperties", properties);

                properties = mp_inst.query<ISetupPropertyStore>();
                VsSetupConfiguration::DebugOutputProperties(L"Instance As PropertyStore", properties);

                ComPtrPackageReference packageReference;
                THROW_IF_FAILED(instance2->GetProduct(&packageReference));
                properties = packageReference.query<ISetupPropertyStore>();
                VsSetupConfiguration::DebugOutputProperties(L"Package Reference", properties);

                ComPtrInstanceCatalog instanceCatalog = mp_inst.query<ISetupInstanceCatalog>();
                THROW_IF_FAILED(instanceCatalog->GetCatalogInfo(&properties));
                VsSetupConfiguration::DebugOutputProperties(L"Instance Catalog", properties);
#endif
            }

        private:
            friend class TerminalApp::VsSetupConfiguration;

            VsSetupInstance(const ComPtrSetupQuery pQuery, const ComPtrSetupInstance pInstance) :
                mp_query(pQuery),
                mp_helper(pQuery.query<ISetupHelper>()),
                mp_inst(pInstance)
            {
            }

            const ComPtrSetupQuery mp_query;
            const ComPtrSetupHelper mp_helper;
            const ComPtrSetupInstance mp_inst;
        };

        static std::vector<VsSetupConfiguration::VsSetupInstance> QueryInstances();

    private:
        VsSetupConfiguration() = default;
        ~VsSetupConfiguration() = default;

        static bool InstallationVersionInRange(ComPtrSetupQuery pQuery, ComPtrSetupInstance pInst, std::wstring_view range);
        static std::wstring ResolvePath(ComPtrSetupInstance pInst, std::wstring_view relativePath);
        static std::wstring GetInstallationVersion(ComPtrSetupInstance pInst);
        static std::wstring GetInstallationPath(ComPtrSetupInstance pInst);
        static std::wstring GetInstanceId(ComPtrSetupInstance pInst);
        static std::wstring GetStringProperty(ComPtrPropertyStore pProps, std::wstring_view name);

#ifdef _DEBUG
        static void DebugOutputProperties(std::wstring_view header, const ComPtrPropertyStore pProps);
#endif
    };
};
