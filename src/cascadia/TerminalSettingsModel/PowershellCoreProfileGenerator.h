/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PowershellCoreProfileGenerator

Abstract:
- This is the dynamic profile generator for PowerShell Core. Checks if pwsh is
  installed, and if it is, creates a profile to launch it.

Author(s):
- Mike Griese - August 2019

--*/

#pragma once

#include "IDynamicProfileGenerator.h"

namespace winrt::Microsoft::Terminal::Settings::Model
{
    class PowershellCoreProfileGenerator final : public IDynamicProfileGenerator
    {
    public:
        static const std::wstring_view GetPreferredPowershellProfileName();

        enum PowerShellFlags
        {
            None = 0,

            // These flags are used as a sort key, so they encode some native ordering.
            // They are ordered such that the "most important" flags have the largest
            // impact on the sort space. For example, since we want Preview to be very polar
            // we give it the highest flag value.
            // The "ideal" powershell instance has 0 flags (stable, native, Program Files location)
            //
            // With this ordering, the sort space ends up being (for PowerShell 6)
            // (numerically greater values are on the left; this is flipped in the final sort)
            //
            // <-- Less Valued .................................... More Valued -->
            // |                 All instances of PS 6                 | All PS7  |
            // |          Preview          |          Stable           | ~~~      |
            // |  Non-Native | Native      |  Non-Native | Native      | ~~~      |
            // | Trd  | Pack | Trd  | Pack | Trd  | Pack | Trd  | Pack | ~~~      |
            // (where Pack is a stand-in for store, scoop, dotnet, though they have their own orders,
            // and Trd is a stand-in for "Traditional" (Program Files))
            //
            // In short, flags with larger magnitudes are pushed further down (therefore valued less)

            // distribution method (choose one)
            Store = 1 << 0, // distributed via the store
            Scoop = 1 << 1, // installed via Scoop
            Dotnet = 1 << 2, // installed as a dotnet global tool
            Traditional = 1 << 3, // installed in traditional Program Files locations

            // native architecture (choose one)
            WOWARM = 1 << 4, // non-native (Windows-on-Windows, ARM variety)
            WOWx86 = 1 << 5, // non-native (Windows-on-Windows, x86 variety)

            // build type (choose one)
            Preview = 1 << 6, // preview version
        };

        struct PowerShellInstance
        {
            int majorVersion; // 0 = we don't know, sort last.
            PowerShellFlags flags;
            std::filesystem::path executablePath;

            constexpr bool operator<(const PowerShellInstance& second) const;
            std::wstring Name() const;
        };

        std::wstring_view GetNamespace() const noexcept override;
        std::wstring_view GetDisplayName() const noexcept override;
        std::wstring_view GetIcon() const noexcept override;
        void GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) override;
        std::vector<PowerShellInstance> GetPowerShellInstances() noexcept;

    private:
        std::vector<PowerShellInstance> _powerShellInstances;
    };
};
