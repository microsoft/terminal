#pragma once

#include "GlobalSettings.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
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
        adjustFontSize,
        closePane,
        closeTab,
        closeWindow,
        copy,
        duplicateTab,
        find,
        moveFocus,
        newTab,
        nextTab,
        openNewTabDropdown,
        openSetting,
        paste,
        prevTab,
        resetFontSize,
        resizePane,
        scrollDown,
        scrollUp,
        scrollUpPage,
        scrollDownPage,
        splitPane,
        switchToTab,
        toggleFullscreen
    };

    class AppSettings
    {
    public:
        AppSettings() = default;
        ~AppSettings() = default;

        AppSettings Clone() { return AppSettings(); }

        // Uncomment these to test some data binding
        // GlobalSettings Globals;
        // std::vector<Profile> Profiles;
        // std::map<hstring, ColorScheme> Schemes;
        std::map<KeyChord, Action> Keybindings;
    };
}
