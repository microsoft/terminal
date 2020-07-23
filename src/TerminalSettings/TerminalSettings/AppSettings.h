#pragma once

#include "GlobalSettings.h"
#include "ColorScheme.h"
#include "Profile.h"

namespace winrt::TerminalSettings::implementation
{
    enum class Modifiers
    {
        Shift = 0x1,
        Alt = 0x2,
        Ctrl = 0x4
    };

    enum class Key
    {
        TODO_FIX
    };

    class KeyChord
    {
        Modifiers Mods;
        Key Button;
    };

    enum class Action
    {
        TODO_FIX
    };

    class AppSettings
    {
        AppSettings();

        GlobalSettings Globals;
        std::vector<Profile> Profiles;
        std::map<hstring, ColorScheme> Schemes;
        std::map<KeyChord, Action> Keybindings;
    };
}
