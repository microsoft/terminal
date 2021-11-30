// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "DynamicProfileUtils.h"
#include "VsDevGDKCmdGenerator.h"
#include <ShlGuid.h>
#include <ShlObj.h>

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

// Method Description:
// - Checks if the Microsoft GDK is installed and if it
//   is, creates profiles to match the shortcuts created by the GDK installer
// Arguments:
// - <none>
// Return Value:
// - a vector with the GDK command prompt profiles
//void VsDevGDKCmdGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
void VsDevGDKCmdGenerator::GenerateProfiles(const VsSetupConfiguration::VsSetupInstance& instance, bool hidden, std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    //try
    //{
    //    if (!IsInstanceValid(instance))
    //    {
    //        return;
    //    }

    //    const auto seed = GetProfileGuidSeed(instance);
    //    const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(seed))) };
    //    auto profile = winrt::make_self<implementation::Profile>(profileGuid);
    //    profile->Name(winrt::hstring{ GetProfileName(instance) });
    //    profile->Commandline(winrt::hstring{ GetProfileCommandLine(instance) });
    //    profile->StartingDirectory(winrt::hstring{ instance.GetInstallationPath() });
    //    profile->Icon(winrt::hstring{ GetProfileIconPath() });
    //    profile->Hidden(hidden);
    //    profiles.emplace_back(std::move(profile));
    //}
    //CATCH_LOG();

    // Fill this with GDK cmd lnk shortcuts that match the VS version
    std::vector<std::filesystem::path> validGDKCmdLnkFiles;

    // Get the productLineVersion prop from the VS install - this should be 2017/2019/etc
    wil::unique_variant vsProductLineVersionProperty;
    if (SUCCEEDED(instance.GetCatalogPropertyStore()->GetValue(L"productLineVersion", &vsProductLineVersionProperty)) && vsProductLineVersionProperty.vt == VT_BSTR)
    {
        if (wcslen(vsProductLineVersionProperty.bstrVal) == 4)
        {
            char s[5];
            wcstombs_s(0, s, vsProductLineVersionProperty.bstrVal, 4);
            std::string vsProductLineVersionPropertyString(s);

            // Find all .lnk files which are used for GDK cmd prompt shortcuts and match these against the productLineVersion
            const std::filesystem::path root{ wil::ExpandEnvironmentStringsW<std::wstring>(L"%GameDK%\\Command Prompts") };
            if (std::filesystem::exists(root))
            {
                for (const auto& cmdPromptLinks : std::filesystem::directory_iterator(root))
                {
                    const auto fullPath = cmdPromptLinks.path();

                    if (fullPath.extension() == ".lnk")
                    {
                        // POSSIBLY FRAGILE!
                        // Extract VS version from .lnk filename
                        auto lnkFilename = fullPath.filename().string();
                        if (lnkFilename.find(vsProductLineVersionPropertyString) != std::wstring::npos)
                        {
                            validGDKCmdLnkFiles.push_back(fullPath);
                        }
                    }
                }
            }
        }
    }

    if (!validGDKCmdLnkFiles.empty())
    {
        IShellLink* pShellLink;
        HRESULT hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink);
        if (SUCCEEDED(hRes))
        {
            if (SUCCEEDED(hRes))
            {
                for (const auto& gdkCmdLinkFile : validGDKCmdLnkFiles)
                {
                    IPersistFile* ppf;
                    hRes = pShellLink->QueryInterface(IID_IPersistFile, (void**)&ppf);
                    hRes = ppf->Load(gdkCmdLinkFile.wstring().c_str(), 0);

                    if (SUCCEEDED(hRes))
                    {
                        WCHAR shellLinkInfo[MAX_PATH];
                        pShellLink->GetDescription(shellLinkInfo, MAX_PATH);

                        const winrt::guid profileGuid{ ::Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(shellLinkInfo))) };
                        auto profile = winrt::make_self<implementation::Profile>(profileGuid);
                        profile->Name(winrt::hstring{ shellLinkInfo });

                        pShellLink->GetPath(shellLinkInfo, MAX_PATH, NULL, 0);
                        std::wstring commandLine(shellLinkInfo);
                        pShellLink->GetArguments(shellLinkInfo, MAX_PATH);
                        commandLine += L" ";
                        commandLine += shellLinkInfo;
                        profile->Commandline(winrt::hstring{ commandLine });

                        pShellLink->GetWorkingDirectory(shellLinkInfo, MAX_PATH);
                        profile->StartingDirectory(winrt::hstring{ shellLinkInfo });


                        profile->Hidden(hidden);

                        profiles.emplace_back(std::move(profile));
                    }

                    ppf->Release();
                }
            }
            pShellLink->Release();
        }


    }
}

