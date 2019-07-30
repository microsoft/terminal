// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include <conattrs.hpp>
#include <io.h>
#include <fcntl.h>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace Microsoft::Console;

// {2bde4a90-d05f-401c-9492-e40884ead1d8}
// uuidv5 properties: name format is UTF-16LE bytes
static constexpr GUID TERMINAL_PROFILE_NAMESPACE_GUID = { 0x2bde4a90, 0xd05f, 0x401c, { 0x94, 0x92, 0xe4, 0x8, 0x84, 0xea, 0xd1, 0xd8 } };

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_PATH{ L"ms-appx:///ProfileIcons/" };
static constexpr std::wstring_view PACKAGED_PROFILE_ICON_EXTENSION{ L".png" };
static constexpr std::wstring_view DEFAULT_LINUX_ICON_GUID{ L"{9acb9455-ca41-5af7-950f-6bca1bc9722f}" };

CascadiaSettings::CascadiaSettings() :
    _globals{},
    _profiles{}
{
}

CascadiaSettings::~CascadiaSettings()
{
}

ColorScheme _CreateCampbellScheme()
{
    ColorScheme campbellScheme{ L"Campbell",
                                RGB(204, 204, 204),
                                RGB(12, 12, 12) };
    auto& campbellTable = campbellScheme.GetTable();
    auto campbellSpan = gsl::span<COLORREF>(&campbellTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    Utils::InitializeCampbellColorTable(campbellSpan);
    Utils::SetColorTableAlpha(campbellSpan, 0xff);

    return campbellScheme;
}

// clang-format off

ColorScheme _CreateVintageScheme()
{
    // as per https://github.com/microsoft/terminal/issues/1781
    ColorScheme vintageScheme { L"Vintage",
                                RGB(192, 192, 192),
                                RGB(  0,   0,   0) };
    auto& vintageTable = vintageScheme.GetTable();
    auto vintageSpan = gsl::span<COLORREF>(&vintageTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    vintageTable[0]  = RGB(  0,   0,   0); // black
    vintageTable[1]  = RGB(128,   0,   0); // dark red
    vintageTable[2]  = RGB(  0, 128,   0); // dark green
    vintageTable[3]  = RGB(128, 128,   0); // dark yellow
    vintageTable[4]  = RGB(  0,   0, 128); // dark blue
    vintageTable[5]  = RGB(128,   0, 128); // dark magenta
    vintageTable[6]  = RGB(  0, 128, 128); // dark cyan
    vintageTable[7]  = RGB(192, 192, 192); // gray
    vintageTable[8]  = RGB(128, 128, 128); // dark gray
    vintageTable[9]  = RGB(255,   0,   0); // red
    vintageTable[10] = RGB(  0, 255,   0); // green
    vintageTable[11] = RGB(255, 255,   0); // yellow
    vintageTable[12] = RGB(  0,   0, 255); // blue
    vintageTable[13] = RGB(255,   0, 255); // magenta
    vintageTable[14] = RGB(  0, 255, 255); // cyan
    vintageTable[15] = RGB(255, 255, 255); // white
    Utils::SetColorTableAlpha(vintageSpan, 0xff);

    return vintageScheme;
}

ColorScheme _CreateOneHalfDarkScheme()
{
    // First 8 dark colors per: https://github.com/sonph/onehalf/blob/master/putty/onehalf-dark.reg
    // Dark gray is per colortool scheme, the other 7 of the last 8 colors from the colortool
    // scheme are the same as their dark color equivalents.
    ColorScheme oneHalfDarkScheme { L"One Half Dark",
                                    RGB(220, 223, 228),
                                    RGB( 40,  44,  52) };
    auto& oneHalfDarkTable = oneHalfDarkScheme.GetTable();
    auto oneHalfDarkSpan = gsl::span<COLORREF>(&oneHalfDarkTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    oneHalfDarkTable[0]  = RGB( 40,  44,  52); // black
    oneHalfDarkTable[1]  = RGB(224, 108, 117); // dark red
    oneHalfDarkTable[2]  = RGB(152, 195, 121); // dark green
    oneHalfDarkTable[3]  = RGB(229, 192, 123); // dark yellow
    oneHalfDarkTable[4]  = RGB( 97, 175, 239); // dark blue
    oneHalfDarkTable[5]  = RGB(198, 120, 221); // dark magenta
    oneHalfDarkTable[6]  = RGB( 86, 182, 194); // dark cyan
    oneHalfDarkTable[7]  = RGB(220, 223, 228); // gray
    oneHalfDarkTable[8]  = RGB( 90,  99, 116); // dark gray
    oneHalfDarkTable[9]  = RGB(224, 108, 117); // red
    oneHalfDarkTable[10] = RGB(152, 195, 121); // green
    oneHalfDarkTable[11] = RGB(229, 192, 123); // yellow
    oneHalfDarkTable[12] = RGB( 97, 175, 239); // blue
    oneHalfDarkTable[13] = RGB(198, 120, 221); // magenta
    oneHalfDarkTable[14] = RGB( 86, 182, 194); // cyan
    oneHalfDarkTable[15] = RGB(220, 223, 228); // white
    Utils::SetColorTableAlpha(oneHalfDarkSpan, 0xff);

    return oneHalfDarkScheme;
}

ColorScheme _CreateOneHalfLightScheme()
{
    // First 8 dark colors per: https://github.com/sonph/onehalf/blob/master/putty/onehalf-light.reg
    // Last 8 colors per colortool scheme.
    ColorScheme oneHalfLightScheme { L"One Half Light",
                                    RGB(56,  58,  66),
                                    RGB(250, 250, 250) };
    auto& oneHalfLightTable = oneHalfLightScheme.GetTable();
    auto oneHalfLightSpan = gsl::span<COLORREF>(&oneHalfLightTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    oneHalfLightTable[0]  = RGB( 56,  58,  66); // black
    oneHalfLightTable[1]  = RGB(228,  86,  73); // dark red
    oneHalfLightTable[2]  = RGB( 80, 161,  79); // dark green
    oneHalfLightTable[3]  = RGB(193, 131,   1); // dark yellow
    oneHalfLightTable[4]  = RGB(  1, 132, 188); // dark blue
    oneHalfLightTable[5]  = RGB(166,  38, 164); // dark magenta
    oneHalfLightTable[6]  = RGB(  9, 151, 179); // dark cyan
    oneHalfLightTable[7]  = RGB(250, 250, 250); // gray
    oneHalfLightTable[8]  = RGB( 79,  82,  93); // dark gray
    oneHalfLightTable[9]  = RGB(223, 108, 117); // red
    oneHalfLightTable[10] = RGB(152, 195, 121); // green
    oneHalfLightTable[11] = RGB(228, 192, 122); // yellow
    oneHalfLightTable[12] = RGB( 97, 175, 239); // blue
    oneHalfLightTable[13] = RGB(197, 119, 221); // magenta
    oneHalfLightTable[14] = RGB( 86, 181, 193); // cyan
    oneHalfLightTable[15] = RGB(255, 255, 255); // white
    Utils::SetColorTableAlpha(oneHalfLightSpan, 0xff);

    return oneHalfLightScheme;
}

ColorScheme _CreateSolarizedDarkScheme()
{
    ColorScheme solarizedDarkScheme { L"Solarized Dark",
                                      RGB(131, 148, 150),
                                      RGB(  0,  43,  54) };
    auto& solarizedDarkTable = solarizedDarkScheme.GetTable();
    auto solarizedDarkSpan = gsl::span<COLORREF>(&solarizedDarkTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    solarizedDarkTable[0]  = RGB(  7, 54, 66);
    solarizedDarkTable[1]  = RGB(220, 50, 47);
    solarizedDarkTable[2]  = RGB(133, 153, 0);
    solarizedDarkTable[3]  = RGB(181, 137, 0);
    solarizedDarkTable[4]  = RGB( 38, 139, 210);
    solarizedDarkTable[5]  = RGB(211, 54, 130);
    solarizedDarkTable[6]  = RGB( 42, 161, 152);
    solarizedDarkTable[7]  = RGB(238, 232, 213);
    solarizedDarkTable[8]  = RGB(  0, 43, 54);
    solarizedDarkTable[9]  = RGB(203, 75, 22);
    solarizedDarkTable[10] = RGB( 88, 110, 117);
    solarizedDarkTable[11] = RGB(101, 123, 131);
    solarizedDarkTable[12] = RGB(131, 148, 150);
    solarizedDarkTable[13] = RGB(108, 113, 196);
    solarizedDarkTable[14] = RGB(147, 161, 161);
    solarizedDarkTable[15] = RGB(253, 246, 227);
    Utils::SetColorTableAlpha(solarizedDarkSpan, 0xff);

    return solarizedDarkScheme;
}

ColorScheme _CreateSolarizedLightScheme()
{
    ColorScheme solarizedLightScheme { L"Solarized Light",
                                       RGB(101, 123, 131),
                                       RGB(253, 246, 227) };
    auto& solarizedLightTable = solarizedLightScheme.GetTable();
    auto solarizedLightSpan = gsl::span<COLORREF>(&solarizedLightTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    solarizedLightTable[0]  = RGB(  7, 54, 66);
    solarizedLightTable[1]  = RGB(220, 50, 47);
    solarizedLightTable[2]  = RGB(133, 153, 0);
    solarizedLightTable[3]  = RGB(181, 137, 0);
    solarizedLightTable[4]  = RGB( 38, 139, 210);
    solarizedLightTable[5]  = RGB(211, 54, 130);
    solarizedLightTable[6]  = RGB( 42, 161, 152);
    solarizedLightTable[7]  = RGB(238, 232, 213);
    solarizedLightTable[8]  = RGB(  0, 43, 54);
    solarizedLightTable[9]  = RGB(203, 75, 22);
    solarizedLightTable[10] = RGB( 88, 110, 117);
    solarizedLightTable[11] = RGB(101, 123, 131);
    solarizedLightTable[12] = RGB(131, 148, 150);
    solarizedLightTable[13] = RGB(108, 113, 196);
    solarizedLightTable[14] = RGB(147, 161, 161);
    solarizedLightTable[15] = RGB(253, 246, 227);
    Utils::SetColorTableAlpha(solarizedLightSpan, 0xff);

    return solarizedLightScheme;
}

// clang-format on

// Method Description:
// - Create the set of schemes to use as the default schemes. Currently creates
//      five default color schemes - Campbell (the new cmd color scheme),
//      One Half Dark, One Half Light, Solarized Dark, and Solarized Light.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_CreateDefaultSchemes()
{
    _globals.GetColorSchemes().emplace_back(_CreateCampbellScheme());
    _globals.GetColorSchemes().emplace_back(_CreateVintageScheme());
    _globals.GetColorSchemes().emplace_back(_CreateOneHalfDarkScheme());
    _globals.GetColorSchemes().emplace_back(_CreateOneHalfLightScheme());
    _globals.GetColorSchemes().emplace_back(_CreateSolarizedDarkScheme());
    _globals.GetColorSchemes().emplace_back(_CreateSolarizedLightScheme());
}

// Method Description:
// - Create a set of profiles to use as the "default" profiles when initializing
//   the terminal. Currently, we create two or three profiles:
//    * one for cmd.exe
//    * one for powershell.exe (inbox Windows Powershell)
//    * if Powershell Core (pwsh.exe) is installed, we'll create another for
//      Powershell Core.
void CascadiaSettings::_CreateDefaultProfiles()
{
    auto cmdProfile{ _CreateDefaultProfile(L"cmd") };
    cmdProfile.SetFontFace(L"Consolas");
    cmdProfile.SetCommandline(L"cmd.exe");
    cmdProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    cmdProfile.SetColorScheme({ L"Campbell" });
    cmdProfile.SetAcrylicOpacity(0.75);
    cmdProfile.SetUseAcrylic(true);

    auto powershellProfile{ _CreateDefaultProfile(L"Windows PowerShell") };
    powershellProfile.SetCommandline(L"powershell.exe");
    powershellProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    powershellProfile.SetColorScheme({ L"Campbell" });
    powershellProfile.SetDefaultBackground(POWERSHELL_BLUE);
    powershellProfile.SetUseAcrylic(false);

    // The Azure connection has a boost dependency, and boost does not support ARM64
    // so we don't create a default profile for the Azure cloud shell if we're in ARM64
#ifndef _M_ARM64
    auto azureCloudShellProfile{ _CreateDefaultProfile(L"Azure Cloud Shell") };
    azureCloudShellProfile.SetCommandline(L"Azure");
    azureCloudShellProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
    azureCloudShellProfile.SetColorScheme({ L"Solarized Dark" });
    azureCloudShellProfile.SetAcrylicOpacity(0.85);
    azureCloudShellProfile.SetUseAcrylic(true);
    azureCloudShellProfile.SetCloseOnExit(false);
    azureCloudShellProfile.SetConnectionType(AzureConnectionType);
#endif

    // If the user has installed PowerShell Core, we add PowerShell Core as a default.
    // PowerShell Core default folder is "%PROGRAMFILES%\PowerShell\[Version]\".
    std::filesystem::path psCoreCmdline{};
    if (_isPowerShellCoreInstalled(psCoreCmdline))
    {
        auto pwshProfile{ _CreateDefaultProfile(L"PowerShell Core") };
        pwshProfile.SetCommandline(psCoreCmdline);
        pwshProfile.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
        pwshProfile.SetColorScheme({ L"Campbell" });

        // If powershell core is installed, we'll use that as the default.
        // Otherwise, we'll use normal Windows Powershell as the default.
        _profiles.emplace_back(pwshProfile);
        _globals.SetDefaultProfile(pwshProfile.GetGuid());
    }
    else
    {
        _globals.SetDefaultProfile(powershellProfile.GetGuid());
    }

    _profiles.emplace_back(powershellProfile);
    _profiles.emplace_back(cmdProfile);
#ifndef _M_ARM64
    _profiles.emplace_back(azureCloudShellProfile);
#endif
    try
    {
        _AppendWslProfiles(_profiles);
    }
    CATCH_LOG()
}

// Method Description:
// - Set up some default keybindings for the terminal.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_CreateDefaultKeybindings()
{
    AppKeyBindings keyBindings = _globals.GetKeybindings();
    // Set up some basic default keybindings
    keyBindings.SetKeyBinding(ShortcutAction::NewTab,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('T') });
    keyBindings.SetKeyBinding(ShortcutAction::DuplicateTab,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('D') });

    keyBindings.SetKeyBinding(ShortcutAction::ClosePane,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('W') });

    keyBindings.SetKeyBinding(ShortcutAction::CopyText,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('C') });

    keyBindings.SetKeyBinding(ShortcutAction::PasteText,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('V') });

    keyBindings.SetKeyBinding(ShortcutAction::OpenSettings,
                              KeyChord{ KeyModifiers::Ctrl,
                                        VK_OEM_COMMA });

    keyBindings.SetKeyBinding(ShortcutAction::NextTab,
                              KeyChord{ KeyModifiers::Ctrl,
                                        VK_TAB });

    keyBindings.SetKeyBinding(ShortcutAction::PrevTab,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_TAB });

    // Yes these are offset by one.
    // Ideally, you'd want C-S-1 to open the _first_ profile, which is index 0
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile0,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('1') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile1,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('2') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile2,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('3') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile3,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('4') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile4,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('5') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile5,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('6') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile6,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('7') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile7,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('8') });
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile8,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('9') });

    keyBindings.SetKeyBinding(ShortcutAction::ScrollUp,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_UP });
    keyBindings.SetKeyBinding(ShortcutAction::ScrollDown,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_DOWN });
    keyBindings.SetKeyBinding(ShortcutAction::ScrollDownPage,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_NEXT });
    keyBindings.SetKeyBinding(ShortcutAction::ScrollUpPage,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_PRIOR });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab0,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('1') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab1,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('2') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab2,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('3') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab3,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('4') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab4,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('5') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab5,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('6') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab6,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('7') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab7,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('8') });
    keyBindings.SetKeyBinding(ShortcutAction::SwitchToTab8,
                              KeyChord{ KeyModifiers::Alt | KeyModifiers::Ctrl,
                                        static_cast<int>('9') });
}

// Method Description:
// - Initialize this object with default color schemes, profiles, and keybindings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::CreateDefaults()
{
    _CreateDefaultProfiles();
    _CreateDefaultSchemes();
    _CreateDefaultKeybindings();
}

// Method Description:
// - Finds a profile that matches the given GUID. If there is no profile in this
//      settings object that matches, returns nullptr.
// Arguments:
// - profileGuid: the GUID of the profile to return.
// Return Value:
// - a non-ownership pointer to the profile matching the given guid, or nullptr
//      if there is no match.
const Profile* CascadiaSettings::FindProfile(GUID profileGuid) const noexcept
{
    for (auto& profile : _profiles)
    {
        if (profile.GetGuid() == profileGuid)
        {
            return &profile;
        }
    }
    return nullptr;
}

// Method Description:
// - Create a TerminalSettings object from the given profile.
//      If the profileGuidArg is not provided, this method will use the default
//      profile.
//   The TerminalSettings object that is created can be used to initialize both
//      the Control's settings, and the Core settings of the terminal.
// Arguments:
// - profileGuidArg: an optional GUID to use to lookup the profile to create the
//      settings from. If this arg is not provided, or the GUID does not match a
//       profile, then this method will use the default profile.
// Return Value:
// - <none>
TerminalSettings CascadiaSettings::MakeSettings(std::optional<GUID> profileGuidArg) const
{
    GUID profileGuid = profileGuidArg ? profileGuidArg.value() : _globals.GetDefaultProfile();
    const Profile* const profile = FindProfile(profileGuid);
    if (profile == nullptr)
    {
        throw E_INVALIDARG;
    }

    TerminalSettings result = profile->CreateTerminalSettings(_globals.GetColorSchemes());

    // Place our appropriate global settings into the Terminal Settings
    _globals.ApplyToSettings(result);

    return result;
}

// Method Description:
// - Returns an iterable collection of all of our Profiles.
// Arguments:
// - <none>
// Return Value:
// - an iterable collection of all of our Profiles.
std::basic_string_view<Profile> CascadiaSettings::GetProfiles() const noexcept
{
    return { &_profiles[0], _profiles.size() };
}

// Method Description:
// - Returns the globally configured keybindings
// Arguments:
// - <none>
// Return Value:
// - the globally configured keybindings
AppKeyBindings CascadiaSettings::GetKeybindings() const noexcept
{
    return _globals.GetKeybindings();
}

// Method Description:
// - Get a reference to our global settings
// Arguments:
// - <none>
// Return Value:
// - a reference to our global settings
GlobalAppSettings& CascadiaSettings::GlobalSettings()
{
    return _globals;
}

// Function Description:
// - Returns true if the user has installed PowerShell Core. This will check
//   both %ProgramFiles% and %ProgramFiles(x86)%, and will return true if
//   powershell core was installed in either location.
// Arguments:
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true iff powershell core (pwsh.exe) is present.
bool CascadiaSettings::_isPowerShellCoreInstalled(std::filesystem::path& cmdline)
{
    return _isPowerShellCoreInstalledInPath(L"%ProgramFiles%", cmdline) ||
           _isPowerShellCoreInstalledInPath(L"%ProgramFiles(x86)%", cmdline);
}

// Function Description:
// - Returns true if the user has installed PowerShell Core.
// Arguments:
// - A string that contains an environment-variable string in the form: %variableName%.
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true iff powershell core (pwsh.exe) is present in the given path
bool CascadiaSettings::_isPowerShellCoreInstalledInPath(const std::wstring_view programFileEnv, std::filesystem::path& cmdline)
{
    std::wstring programFileEnvNulTerm{ programFileEnv };
    std::filesystem::path psCorePath{ wil::ExpandEnvironmentStringsW<std::wstring>(programFileEnvNulTerm.data()) };
    psCorePath /= L"PowerShell";
    if (std::filesystem::exists(psCorePath))
    {
        for (auto& p : std::filesystem::directory_iterator(psCorePath))
        {
            psCorePath = p.path();
            psCorePath /= L"pwsh.exe";
            if (std::filesystem::exists(psCorePath))
            {
                cmdline = psCorePath;
                return true;
            }
        }
    }
    return false;
}

// Function Description:
// - Adds all of the WSL profiles to the provided container.
// Arguments:
// - A ref to the profiles container where the WSL profiles are to be added
// Return Value:
// - <none>
void CascadiaSettings::_AppendWslProfiles(std::vector<TerminalApp::Profile>& profileStorage)
{
    wil::unique_handle readPipe;
    wil::unique_handle writePipe;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&readPipe, &writePipe, &sa, 0));
    STARTUPINFO si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe.get();
    si.hStdError = writePipe.get();
    wil::unique_process_information pi{};
    wil::unique_cotaskmem_string systemPath;
    THROW_IF_FAILED(wil::GetSystemDirectoryW(systemPath));
    std::wstring command(systemPath.get());
    command += L"\\wsl.exe --list";

    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
                                             const_cast<LPWSTR>(command.c_str()),
                                             nullptr,
                                             nullptr,
                                             TRUE,
                                             CREATE_NO_WINDOW,
                                             nullptr,
                                             nullptr,
                                             &si,
                                             &pi));
    switch (WaitForSingleObject(pi.hProcess, INFINITE))
    {
    case WAIT_OBJECT_0:
        break;
    case WAIT_ABANDONED:
    case WAIT_TIMEOUT:
        THROW_HR(ERROR_CHILD_NOT_COMPLETE);
    case WAIT_FAILED:
        THROW_LAST_ERROR();
    default:
        THROW_HR(ERROR_UNHANDLED_EXCEPTION);
    }
    DWORD exitCode;
    if (GetExitCodeProcess(pi.hProcess, &exitCode) == false)
    {
        THROW_HR(E_INVALIDARG);
    }
    else if (exitCode != 0)
    {
        return;
    }
    DWORD bytesAvailable;
    THROW_IF_WIN32_BOOL_FALSE(PeekNamedPipe(readPipe.get(), nullptr, NULL, nullptr, &bytesAvailable, nullptr));
    std::wfstream pipe{ _wfdopen(_open_osfhandle((intptr_t)readPipe.get(), _O_WTEXT | _O_RDONLY), L"r") };
    // don't worry about the handle returned from wfdOpen, readPipe handle is already managed by wil
    // and closing the file handle will cause an error.
    std::wstring wline;
    std::getline(pipe, wline); // remove the header from the output.
    while (pipe.tellp() < bytesAvailable)
    {
        std::getline(pipe, wline);
        std::wstringstream wlinestream(wline);
        if (wlinestream)
        {
            std::wstring distName;
            std::getline(wlinestream, distName, L'\r');
            size_t firstChar = distName.find_first_of(L"( ");
            // Some localizations don't have a space between the name and "(Default)"
            // https://github.com/microsoft/terminal/issues/1168#issuecomment-500187109
            if (firstChar < distName.size())
            {
                distName.resize(firstChar);
            }
            auto WSLDistro{ _CreateDefaultProfile(distName) };
            WSLDistro.SetCommandline(L"wsl.exe -d " + distName);
            WSLDistro.SetColorScheme({ L"Campbell" });
            WSLDistro.SetStartingDirectory(DEFAULT_STARTING_DIRECTORY);
            std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
            iconPath.append(DEFAULT_LINUX_ICON_GUID);
            iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);
            WSLDistro.SetIconPath(iconPath);
            profileStorage.emplace_back(WSLDistro);
        }
    }
}

// Method Description:
// - Helper function for creating a skeleton default profile with a pre-populated
//   guid and name.
// Arguments:
// - name: the name of the new profile.
// Return Value:
// - A Profile, ready to be filled in
Profile CascadiaSettings::_CreateDefaultProfile(const std::wstring_view name)
{
    auto profileGuid{ Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, gsl::as_bytes(gsl::make_span(name))) };
    Profile newProfile{ profileGuid };

    newProfile.SetName(static_cast<std::wstring>(name));

    std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    iconPath.append(Utils::GuidToString(profileGuid));
    iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);

    newProfile.SetIconPath(iconPath);

    return newProfile;
}
