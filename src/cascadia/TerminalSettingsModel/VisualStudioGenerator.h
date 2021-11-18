/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VisualStudioGenerator

Abstract:
- Generator for Visual Studio shell profiles. Actual profile generation is delegated
  to separate classes that encapsulate different logic for cmd- and powershell-based shells,
  as well as VC startup scripts specific to the current processor architecture.

Author(s):
- Charles Willis - October 2020
- Heath Stewart - September 2021

--*/

#pragma once

#include "IDynamicProfileGenerator.h"
#include "VsSetupConfiguration.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class VisualStudioGenerator : public IDynamicProfileGenerator
    {
    public:
        std::wstring_view GetNamespace() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const override;

        class IVisualStudioProfileGenerator
        {
        public:
            virtual void GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const = 0;
        };
    };
};
