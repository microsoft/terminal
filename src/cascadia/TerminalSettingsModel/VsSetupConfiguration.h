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
            std::wstring ResolvePath(std::wstring_view relativePath) const
            {
                return VsSetupConfiguration::ResolvePath(inst.get(), relativePath);
            }

            std::wstring GetDevShellModulePath() const
            {
                // The path of Microsoft.VisualStudio.DevShell.dll changed in 16.3
                if (VersionInRange(L"[16.3,"))
                {
                    return ResolvePath(L"Common7\\Tools\\Microsoft.VisualStudio.DevShell.dll");
                }

                return ResolvePath(L"Common7\\Tools\\vsdevshell\\Microsoft.VisualStudio.DevShell.dll");
            }

            std::wstring GetDevCmdScriptPath() const
            {
                return ResolvePath(L"Common7\\Tools\\VsDevCmd.bat");
            }

            bool VersionInRange(std::wstring_view range) const
            {
                return InstallationVersionInRange(query.get(), inst.get(), range);
            }

            std::wstring GetVersion() const
            {
                return GetInstallationVersion(inst.get());
            }

            std::wstring GetInstallationPath() const
            {
                return VsSetupConfiguration::GetInstallationPath(inst.get());
            }

            std::wstring GetInstanceId() const
            {
                return VsSetupConfiguration::GetInstanceId(inst.get());
            }

            ComPtrPropertyStore GetInstancePropertyStore() const
            {
                ComPtrPropertyStore properties;
                inst.query_to<ISetupPropertyStore>(&properties);
                return properties;
            }

            ComPtrCustomPropertyStore GetCustomPropertyStore() const
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

            ComPtrCatalogPropertyStore GetCatalogPropertyStore() const
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

            std::wstring GetProfileNameSuffix() const
            {
                return profileNameSuffix;
            }

        private:
            friend class VsSetupConfiguration;

            VsSetupInstance(ComPtrSetupQuery pQuery, ComPtrSetupInstance pInstance) :
                query(std::move(pQuery)),
                inst(std::move(pInstance)),
                profileNameSuffix(BuildProfileNameSuffix())
            {
            }

            ComPtrSetupQuery query;
            ComPtrSetupInstance inst;

            std::wstring profileNameSuffix;

            std::wstring BuildProfileNameSuffix() const
            {
                ComPtrCatalogPropertyStore catalogProperties = GetCatalogPropertyStore();
                if (catalogProperties != nullptr)
                {
                    std::wstring suffix;

                    std::wstring productLine{ GetProductLineVersion(catalogProperties.get()) };
                    suffix.append(productLine);

                    ComPtrCustomPropertyStore customProperties = GetCustomPropertyStore();
                    if (customProperties != nullptr)
                    {
                        std::wstring nickname{ GetNickname(customProperties.get()) };
                        if (!nickname.empty())
                        {
                            suffix.append(L" (" + nickname + L")");
                        }
                        else
                        {
                            ComPtrPropertyStore instanceProperties = GetInstancePropertyStore();
                            suffix.append(GetChannelNameSuffixTag(instanceProperties.get()));
                        }
                    }
                    else
                    {
                        ComPtrPropertyStore instanceProperties = GetInstancePropertyStore();
                        suffix.append(GetChannelNameSuffixTag(instanceProperties.get()));
                    }

                    return suffix;
                }

                return GetVersion();
            }

            static std::wstring GetChannelNameSuffixTag(ISetupPropertyStore* instanceProperties)
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

            static std::wstring GetChannelId(ISetupPropertyStore* instanceProperties)
            {
                return GetStringProperty(instanceProperties, L"channelId");
            }

            static std::wstring GetChannelName(ISetupPropertyStore* instanceProperties)
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

            static std::wstring GetNickname(ISetupPropertyStore* customProperties)
            {
                return GetStringProperty(customProperties, L"nickname");
            }

            static std::wstring GetProductLineVersion(ISetupPropertyStore* customProperties)
            {
                return GetStringProperty(customProperties, L"productLineVersion");
            }
        };

        static std::vector<VsSetupInstance> QueryInstances();

    private:
        static bool InstallationVersionInRange(ISetupConfiguration2* pQuery, ISetupInstance* pInst, std::wstring_view range);
        static std::wstring ResolvePath(ISetupInstance* pInst, std::wstring_view relativePath);
        static std::wstring GetInstallationVersion(ISetupInstance* pInst);
        static std::wstring GetInstallationPath(ISetupInstance* pInst);
        static std::wstring GetInstanceId(ISetupInstance* pInst);
        static std::wstring GetStringProperty(ISetupPropertyStore* pProps, std::wstring_view name);
    };
};
