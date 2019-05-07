// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <argb.h>
#include <conattrs.hpp>
#include "CascadiaSettings.h"
#include "../../types/inc/utils.hpp"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace ::TerminalApp;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;

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
    ColorScheme campbellScheme { L"Campbell",
                                 RGB(242, 242, 242),
                                 RGB(12, 12, 12) };
    auto& campbellTable = campbellScheme.GetTable();
    auto campbellSpan = gsl::span<COLORREF>(&campbellTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    Microsoft::Console::Utils::InitializeCampbellColorTable(campbellSpan);
    Microsoft::Console::Utils::SetColorTableAlpha(campbellSpan, 0xff);

    return campbellScheme;
}

ColorScheme _CreateSolarizedDarkScheme()
{

    ColorScheme solarizedDarkScheme { L"Solarized Dark",
                                      RGB(253, 246, 227),
                                      RGB(  7, 54,  66) };
    auto& solarizedDarkTable = solarizedDarkScheme.GetTable();
    auto solarizedDarkSpan = gsl::span<COLORREF>(&solarizedDarkTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    solarizedDarkTable[0]  = RGB(  7, 54, 66);
    solarizedDarkTable[1]  = RGB(211, 1, 2);
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
    Microsoft::Console::Utils::SetColorTableAlpha(solarizedDarkSpan, 0xff);

    return solarizedDarkScheme;
}

ColorScheme _CreateSolarizedLightScheme()
{
    ColorScheme solarizedLightScheme { L"Solarized Light",
                                       RGB(  7, 54,  66),
                                       RGB(253, 246, 227) };
    auto& solarizedLightTable = solarizedLightScheme.GetTable();
    auto solarizedLightSpan = gsl::span<COLORREF>(&solarizedLightTable[0], gsl::narrow<ptrdiff_t>(COLOR_TABLE_SIZE));
    solarizedLightTable[0]  = RGB(  7, 54, 66);
    solarizedLightTable[1]  = RGB(211, 1, 2);
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
    Microsoft::Console::Utils::SetColorTableAlpha(solarizedLightSpan, 0xff);

    return solarizedLightScheme;
}

// Method Description:
// - Create the set of schemes to use as the default schemes. Currently creates
//      three default color schemes - Campbell (the new cmd color scheme),
//      Solarized Dark and Solarized Light.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_CreateDefaultSchemes()
{
    _globals.GetColorSchemes().emplace_back(_CreateCampbellScheme());
    _globals.GetColorSchemes().emplace_back(_CreateSolarizedDarkScheme());
    _globals.GetColorSchemes().emplace_back(_CreateSolarizedLightScheme());

}

// Method Description:
// - Create a set of profiles to use as the "default" profiles when initializing
//      the terminal. Currently, we create two profiles: one for cmd.exe, and
//      one for powershell.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_CreateDefaultProfiles()
{
    Profile defaultProfile{};
    defaultProfile.SetFontFace(L"Consolas");
    defaultProfile.SetCommandline(L"cmd.exe");
    defaultProfile.SetColorScheme({ L"Campbell" });
    defaultProfile.SetAcrylicOpacity(0.75);
    defaultProfile.SetUseAcrylic(true);
    defaultProfile.SetName(L"cmd");

    _globals.SetDefaultProfile(defaultProfile.GetGuid());

    Profile powershellProfile{};
    // If the user has installed PowerShell Core, we add PowerShell Core as a default.
    // PowerShell Core default folder is "%PROGRAMFILES%\PowerShell\[Version]\".
    std::wstring psCmdline = L"powershell.exe";
    std::filesystem::path psCoreCmdline{};
    if (_IsPowerShellCoreInstalled(L"%ProgramFiles%", psCoreCmdline))
    {
        psCmdline = psCoreCmdline;
    }
    else if (_IsPowerShellCoreInstalled(L"%ProgramFiles(x86)%", psCoreCmdline))
    {
        psCmdline = psCoreCmdline;
    }
    powershellProfile.SetFontFace(L"Courier New");
    powershellProfile.SetCommandline(psCmdline);
    powershellProfile.SetColorScheme({ L"Campbell" });
    powershellProfile.SetDefaultBackground(RGB(1, 36, 86));
    powershellProfile.SetUseAcrylic(false);
    powershellProfile.SetName(L"PowerShell");

    _profiles.emplace_back(defaultProfile);
    _profiles.emplace_back(powershellProfile);
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
    // Set up spme basic default keybindings
    // TODO:MSFT:20700157 read our settings from some source, and configure
    //      keychord,action pairings from that file
    keyBindings.SetKeyBinding(ShortcutAction::NewTab,
                               KeyChord{ KeyModifiers::Ctrl,
                                         static_cast<int>('T') });

    keyBindings.SetKeyBinding(ShortcutAction::CloseTab,
                               KeyChord{ KeyModifiers::Ctrl,
                                         static_cast<int>('W') });

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
    keyBindings.SetKeyBinding(ShortcutAction::NewTabProfile9,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        static_cast<int>('0') });

    keyBindings.SetKeyBinding(ShortcutAction::ScrollUp,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_PRIOR });
    keyBindings.SetKeyBinding(ShortcutAction::ScrollDown,
                              KeyChord{ KeyModifiers::Ctrl | KeyModifiers::Shift,
                                        VK_NEXT });
}

// Method Description:
// - Initialize this object with default color schemes, profiles, and keybindings.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CascadiaSettings::_CreateDefaults()
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
// - Returns true if the user has installed PowerShell Core.
// Arguments:
// - A string that contains an environment-variable string in the form: %variableName%.
// - A ref of a path that receives the result of PowerShell Core pwsh.exe full path.
// Return Value:
// - true or false.
bool CascadiaSettings::_IsPowerShellCoreInstalled(std::wstring_view programFileEnv, std::filesystem::path& cmdline)
{
    std::filesystem::path psCorePath = ExpandEnvironmentVariableString(programFileEnv.data());
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
// - Get a environment variable string.
// Arguments:
// - A string that contains an environment-variable string in the form: %variableName%.
// Return Value:
// - a string of the expending environment-variable string.
std::wstring CascadiaSettings::ExpandEnvironmentVariableString(std::wstring_view source)
{
    std::wstring result{};
    DWORD requiredSize = 0;
    do
    {
        result.resize(requiredSize);
        requiredSize = ::ExpandEnvironmentStringsW(source.data(), result.data(), static_cast<DWORD>(result.size()));
    } while (requiredSize != result.size());

    // Trim the terminating null character
    result.resize(requiredSize-1);
    return result;
}
