/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VsSetupConfiguration

Abstract:
- Encapsulates the Visual Studio Setup Configuration COM APIs

Author(s):
- Charles Willis - October 2020
- Heath Stewart - September 2021

--*/

#pragma once

#include "Setup.Configuration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
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
            VsSetupInstance(VsSetupInstance&& other) = default;
            VsSetupInstance& operator=(VsSetupInstance&&) = default;

            std::wstring ResolvePath(std::wstring_view relativePath) const
            {
                return VsSetupConfiguration::ResolvePath(inst.get(), relativePath);
            }

            bool VersionInRange(std::wstring_view range) const
            {
                return InstallationVersionInRange(query.get(), inst.get(), range);
            }

            std::wstring GetVersion() const
            {
                return VsSetupConfiguration::GetInstallationVersion(inst.get());
            }

            unsigned long long GetComparableInstallDate() const noexcept
            {
                return installDate;
            }

            unsigned long long GetComparableVersion() const noexcept
            {
                return version;
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

            VsSetupInstance(ComPtrSetupQuery pQuery, ComPtrSetupInstance pInstance) :
                query(pQuery), // Copy and AddRef the query object.
                inst(std::move(pInstance)), // Take ownership of the instance object.
                profileNameSuffix(BuildProfileNameSuffix()),
                installDate(GetInstallDate()),
                version(GetInstallationVersion())
            {
            }

        private:
            ComPtrSetupQuery query;
            ComPtrSetupInstance inst;

            std::wstring profileNameSuffix;

            // Cache oft-accessed properties used in sorting.
            unsigned long long installDate;
            unsigned long long version;

            std::wstring BuildProfileNameSuffix() const
            {
                auto catalogProperties = GetCatalogPropertyStore();
                if (catalogProperties != nullptr)
                {
                    std::wstring suffix;

                    auto productLine{ GetProductLineVersion(catalogProperties.get()) };
                    suffix.append(productLine);

                    auto customProperties = GetCustomPropertyStore();
                    if (customProperties != nullptr)
                    {
                        auto nickname{ GetNickname(customProperties.get()) };
                        if (!nickname.empty())
                        {
                            suffix.append(L" (" + nickname + L")");
                        }
                        else
                        {
                            auto instanceProperties = GetInstancePropertyStore();
                            suffix.append(GetChannelNameSuffixTag(instanceProperties.get()));
                        }
                    }
                    else
                    {
                        auto instanceProperties = GetInstancePropertyStore();
                        suffix.append(GetChannelNameSuffixTag(instanceProperties.get()));
                    }

                    return suffix;
                }

                return GetVersion();
            }

            unsigned long long GetInstallDate() const
            {
                return VsSetupConfiguration::GetInstallDate(inst.get());
            }

            unsigned long long GetInstallationVersion() const
            {
                const auto helper = wil::com_query<ISetupHelper>(query);

                auto versionString{ GetVersion() };
                unsigned long long version{ 0 };

                THROW_IF_FAILED(helper->ParseVersion(versionString.data(), &version));
                return version;
            }

            static std::wstring GetChannelNameSuffixTag(ISetupPropertyStore* instanceProperties)
            {
                std::wstring tag;
                auto channelName{ GetChannelName(instanceProperties) };

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
                auto channelId{ GetChannelId(instanceProperties) };
                if (channelId.empty())
                {
                    return channelId;
                }

                std::wstring channelName;

                // channelId is in the format  <ProductName>.<MajorVersion>.<ChannelName>
                auto pos = channelId.rfind(L".");
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
        static unsigned long long GetInstallDate(ISetupInstance* pInst);
        static std::wstring GetStringProperty(ISetupPropertyStore* pProps, std::wstring_view name);
    };
};
