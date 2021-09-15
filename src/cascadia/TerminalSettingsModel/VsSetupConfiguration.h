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

#include "Setup.Configuration.h"

namespace Microsoft::Terminal::Settings::Model
{
    /// <summary>
    /// See https://docs.microsoft.com/en-us/dotnet/api/microsoft.visualstudio.setup.configuration?view=visualstudiosdk-2019
    /// </summary>
    class VsSetupConfiguration
    {
    private:
        typedef wil::com_ptr<ISetupConfiguration2> ComPtrSetupQuery;
        typedef wil::com_ptr<ISetupHelper> ComPtrSetupHelper;
        typedef wil::com_ptr<ISetupInstance> ComPtrSetupInstance;
        typedef wil::com_ptr<ISetupInstance2> ComPtrSetupInstance2;
        typedef wil::com_ptr<ISetupPropertyStore> ComPtrPropertyStore;
        typedef wil::com_ptr<ISetupPackageReference> ComPtrPackageReference;
        typedef wil::com_ptr<ISetupInstanceCatalog> ComPtrInstanceCatalog;
        typedef ComPtrPropertyStore ComPtrCustomPropertyStore;
        typedef ComPtrPropertyStore ComPtrCatalogPropertyStore;

    public:
        struct VsSetupInstance
        {
            inline std::wstring ResolvePath(std::wstring_view relativePath) const
            {
                return VsSetupConfiguration::ResolvePath(inst, relativePath);
            }

            inline std::wstring GetDevShellModulePath() const
            {
                // The path of Microsoft.VisualStudio.DevShell.dll changed in 16.3
                if (VersionInRange(L"[16.3,"))
                {
                    return ResolvePath(L"Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll");
                }

                return ResolvePath(L"Common7\\Tools\\vsdevshell\\Microsoft.VisualStudio.DevShell.dll");
            }

            inline std::wstring GetDevCmdScriptPath() const
            {
                return ResolvePath(L"Common7\\Tools\\VsDevCmd.bat");
            }

            inline bool VersionInRange(std::wstring_view range) const
            {
                return VsSetupConfiguration::InstallationVersionInRange(query, inst, range);
            }

            inline std::wstring GetVersion() const
            {
                return VsSetupConfiguration::GetInstallationVersion(inst);
            }

            inline std::wstring GetInstallationPath() const
            {
                return VsSetupConfiguration::GetInstallationPath(inst);
            }

            inline std::wstring GetInstanceId() const
            {
                return VsSetupConfiguration::GetInstanceId(inst);
            }

            inline ComPtrPropertyStore GetInstancePropertyStore() const
            {
                ComPtrPropertyStore properties;
                inst.query_to<ISetupPropertyStore>(&properties);
                return properties;
            }

            inline ComPtrCustomPropertyStore GetCustomPropertyStore() const
            {
                ComPtrSetupInstance2 instance2;
                inst.query_to<ISetupInstance2>(&instance2);
                ComPtrCustomPropertyStore properties;
                if (FAILED(instance2->GetProperties(&properties)))
                {
                    return nullptr;
                }

                return properties;
            }

            inline ComPtrCatalogPropertyStore GetCatalogPropertyStore() const
            {
                ComPtrInstanceCatalog instanceCatalog;
                inst.query_to<ISetupInstanceCatalog>(&instanceCatalog);
                ComPtrCatalogPropertyStore properties;
                if (FAILED(instanceCatalog->GetCatalogInfo(&properties)))
                {
                    return nullptr;
                }

                return properties;
            }

            inline std::wstring GetProfileNameSuffix() const
            {
                return profileNameSuffix;
            }

        private:
            friend class VsSetupConfiguration;

            VsSetupInstance(const ComPtrSetupQuery pQuery, const ComPtrSetupInstance pInstance) :
                query(pQuery),
                helper(pQuery.query<ISetupHelper>()),
                inst(pInstance),
                profileNameSuffix(BuildProfileNameSuffix())
            {
            }

            const ComPtrSetupQuery query;
            const ComPtrSetupHelper helper;
            const ComPtrSetupInstance inst;

            std::wstring profileNameSuffix;

            inline std::wstring BuildProfileNameSuffix() const
            {
                ComPtrCatalogPropertyStore catalogProperties = GetCatalogPropertyStore();
                if (catalogProperties != nullptr)
                {
                    std::wstring suffix;

                    std::wstring productLine{ GetProductLineVersion(catalogProperties) };
                    suffix.append(productLine);

                    ComPtrCustomPropertyStore customProperties = GetCustomPropertyStore();
                    if (customProperties != nullptr)
                    {
                        std::wstring nickname{ GetNickname(customProperties) };
                        if (!nickname.empty())
                        {
                            suffix.append(L" (" + nickname + L")");
                        }
                        else
                        {
                            ComPtrPropertyStore instanceProperties = GetInstancePropertyStore();
                            suffix.append(GetChannelNameSuffixTag(instanceProperties));
                        }
                    }
                    else
                    {
                        ComPtrPropertyStore instanceProperties = GetInstancePropertyStore();
                        suffix.append(GetChannelNameSuffixTag(instanceProperties));
                    }

                    return suffix;
                }

                return GetVersion();
            }

            inline std::wstring GetChannelNameSuffixTag(ComPtrPropertyStore instanceProperties) const
            {
                std::wstring tag;
                std::wstring channelName{ GetChannelName(instanceProperties) };

                if (channelName.empty())
                {
                    return channelName;
                }

                if (channelName != L"Release")
                {
                    tag.append(L" [" + channelName + L"]");
                }

                return tag;
            }

            inline std::wstring GetChannelId(ComPtrPropertyStore instanceProperties) const
            {
                return VsSetupConfiguration::GetStringProperty(instanceProperties, L"channelId");
            }

            inline std::wstring GetChannelName(ComPtrPropertyStore instanceProperties) const
            {
                std::wstring channelId{ GetChannelId(instanceProperties) };
                if (channelId.empty())
                {
                    return channelId;
                }

                std::wstring channelName;

                // channelId is in the format  <ProductName>.<MajorVersion>.<ChannelName>
                size_t pos = channelId.rfind(L".");
                if (pos != std::wstring::npos)
                {
                    channelName.append(channelId.substr(pos + 1));
                }

                return channelName;
            }

            inline std::wstring GetNickname(ComPtrCustomPropertyStore customProperties) const
            {
                return VsSetupConfiguration::GetStringProperty(customProperties, L"nickname");
            }

            inline std::wstring GetProductLineVersion(ComPtrCatalogPropertyStore customProperties) const
            {
                return VsSetupConfiguration::GetStringProperty(customProperties, L"productLineVersion");
            }
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
    };
};
