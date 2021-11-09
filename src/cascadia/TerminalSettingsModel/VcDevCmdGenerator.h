/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VcDevCmdGenerator

Abstract:
- Dynamic profile generator for Visual C++ development environments.

Author(s):
- Heath Stewart - September 2021

--*/

#pragma once
#include "VisualStudioGenerator.h"
#include "VsSetupConfiguration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class VcDevCmdGenerator final : public VisualStudioGenerator::IVisualStudioProfileGenerator
    {
    public:
        void GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

    private:
        winrt::com_ptr<implementation::Profile> CreateProfile(const VsSetupConfiguration::VsSetupInstance& instance, const std::wstring_view& prefix, const std::filesystem::path& path, bool hidden) const;

        std::wstring GetProfileIconPath() const
        {
            return L"ms-appx:///ProfileIcons/{0caa0dad-35be-5f56-a8ff-afceeeaa6101}.png";
        }

        std::wstring GetProfileGuidSeed(const VsSetupConfiguration::VsSetupInstance& instance, const std::filesystem::path& path) const;
        std::wstring GetProfileName(const VsSetupConfiguration::VsSetupInstance& instance, const std::wstring_view& prefix) const;
        std::wstring GetProfileCommandLine(const std::filesystem::path& path) const;
        std::wstring GetVcCmdScriptDirectory(const VsSetupConfiguration::VsSetupInstance& instance) const;
    };
};
