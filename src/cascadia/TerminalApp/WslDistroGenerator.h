/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
-

Abstract:
-
Author(s):
- Mike Griese - August 2019

--*/

#pragma once
// #include <winrt/Microsoft.Terminal.TerminalConnection.h>
// #include "GlobalAppSettings.h"
// #include "TerminalWarnings.h"
#include "IDynamicProfileGenerator.h"

// // fwdecl unittest classes
// namespace TerminalAppLocalTests
// {
//     class SettingsTests;
// }

namespace TerminalApp
{
    // class IDynamicProfileGenerator;
    // class PowershellCoreProfileGenerator;
    class WslDistroGenerator;
    // class IGuidProvder;
};

class TerminalApp::WslDistroGenerator : public TerminalApp::IDynamicProfileGenerator
{
public:
    WslDistroGenerator() = default;
    ~WslDistroGenerator() = default;
    std::wstring GetNamespace() override;
    // {
    //     return nullptr;
    // }

    std::vector<TerminalApp::Profile> GenerateProfiles() override;
    // {
    //     std::vector<TerminalApp::Profile> profiles{};

    //     wil::unique_handle readPipe;
    //     wil::unique_handle writePipe;
    //     SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
    //     THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&readPipe, &writePipe, &sa, 0));
    //     STARTUPINFO si{};
    //     si.cb = sizeof(si);
    //     si.dwFlags = STARTF_USESTDHANDLES;
    //     si.hStdOutput = writePipe.get();
    //     si.hStdError = writePipe.get();
    //     wil::unique_process_information pi{};
    //     wil::unique_cotaskmem_string systemPath;
    //     THROW_IF_FAILED(wil::GetSystemDirectoryW(systemPath));
    //     std::wstring command(systemPath.get());
    //     command += L"\\wsl.exe --list";

    //     THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
    //                                              const_cast<LPWSTR>(command.c_str()),
    //                                              nullptr,
    //                                              nullptr,
    //                                              TRUE,
    //                                              CREATE_NO_WINDOW,
    //                                              nullptr,
    //                                              nullptr,
    //                                              &si,
    //                                              &pi));
    //     switch (WaitForSingleObject(pi.hProcess, INFINITE))
    //     {
    //     case WAIT_OBJECT_0:
    //         break;
    //     case WAIT_ABANDONED:
    //     case WAIT_TIMEOUT:
    //         THROW_HR(ERROR_CHILD_NOT_COMPLETE);
    //     case WAIT_FAILED:
    //         THROW_LAST_ERROR();
    //     default:
    //         THROW_HR(ERROR_UNHANDLED_EXCEPTION);
    //     }
    //     DWORD exitCode;
    //     if (GetExitCodeProcess(pi.hProcess, &exitCode) == false)
    //     {
    //         THROW_HR(E_INVALIDARG);
    //     }
    //     else if (exitCode != 0)
    //     {
    //         return;
    //     }
    //     DWORD bytesAvailable;
    //     THROW_IF_WIN32_BOOL_FALSE(PeekNamedPipe(readPipe.get(), nullptr, NULL, nullptr, &bytesAvailable, nullptr));
    //     std::wfstream pipe{ _wfdopen(_open_osfhandle((intptr_t)readPipe.get(), _O_WTEXT | _O_RDONLY), L"r") };
    //     // don't worry about the handle returned from wfdOpen, readPipe handle is already managed by wil
    //     // and closing the file handle will cause an error.
    //     std::wstring wline;
    //     std::getline(pipe, wline); // remove the header from the output.
    //     while (pipe.tellp() < bytesAvailable)
    //     {
    //         std::getline(pipe, wline);
    //         std::wstringstream wlinestream(wline);
    //         if (wlinestream)
    //         {
    //             std::wstring distName;
    //             std::getline(wlinestream, distName, L'\r');
    //             size_t firstChar = distName.find_first_of(L"( ");
    //             // Some localizations don't have a space between the name and "(Default)"
    //             // https://github.com/microsoft/terminal/issues/1168#issuecomment-500187109
    //             if (firstChar < distName.size())
    //             {
    //                 distName.resize(firstChar);
    //             }
    //             // auto WSLDistro{ _CreateDefaultProfile(distName) };
    //             TerminalApp::Profile WSLDistro{ 0 };
    //             WSLDistro.SetName(distName);
    //             WSLDistro.SetCommandline(L"wsl.exe -d " + distName);
    //             WSLDistro.SetColorScheme({ L"Campbell" });
    //             WSLDistro.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    //             // std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    //             // iconPath.append(DEFAULT_LINUX_ICON_GUID);
    //             // iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);
    //             // WSLDistro.SetIconPath(iconPath);
    //             WSLDistro.SetIconPath(L"ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png");
    //             profiles.emplace_back(WSLDistro);
    //         }
    //     }

    //     return profiles;
    // }
};
