/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ColorScheme.hpp

Abstract:
- A color scheme is a single set of colors to use as the terminal colors. These
    schemes are named, and can be used to quickly change all the colors of the
    terminal to another scheme.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once
#include "../../inc/conattrs.hpp"
#include "inc/cppwinrt_utils.h"

#include "ColorScheme.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class SettingsTests;
    class ColorSchemeTests;
};

namespace winrt::TerminalApp::implementation
{
// Use this macro to quick implement both the getter and setter for a color property.
// This should only be used for color types where there's no logic in the
// getter/setter beyond just accessing/updating the value.
// This takes advantage of til::color
#define GETSET_COLORTABLEPROPERTY(name)                                               \
public:                                                                               \
    winrt::Windows::UI::Color name() const noexcept { return _table[##name##Index]; } \
    void name(const winrt::Windows::UI::Color& value) noexcept { _table[##name##Index] = value; }

    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
    public:
        ColorScheme() = default;
        ColorScheme(hstring name, Windows::UI::Color defaultFg, Windows::UI::Color defaultBg, Windows::UI::Color cursorColor);

        static com_ptr<ColorScheme> FromJson(const Json::Value& json);
        bool ShouldBeLayered(const Json::Value& json) const;
        void LayerJson(const Json::Value& json);

        static Json::Value ToJson(const TerminalApp::ColorScheme& scheme);
        void UpdateJson(Json::Value& json);

        static std::optional<std::wstring> GetNameFromJson(const Json::Value& json);

        com_array<Windows::UI::Color> Table() const noexcept;

        GETSET_PROPERTY(winrt::hstring, Name, L"");
        GETSET_COLORPROPERTY(Foreground, DEFAULT_FOREGROUND_WITH_ALPHA);
        GETSET_COLORPROPERTY(Background, DEFAULT_BACKGROUND_WITH_ALPHA);
        GETSET_COLORPROPERTY(SelectionBackground, DEFAULT_FOREGROUND);
        GETSET_COLORPROPERTY(CursorColor, DEFAULT_CURSOR_COLOR);

        GETSET_COLORTABLEPROPERTY(Black);
        GETSET_COLORTABLEPROPERTY(Red);
        GETSET_COLORTABLEPROPERTY(Green);
        GETSET_COLORTABLEPROPERTY(Yellow);
        GETSET_COLORTABLEPROPERTY(Blue);
        GETSET_COLORTABLEPROPERTY(Purple);
        GETSET_COLORTABLEPROPERTY(Cyan);
        GETSET_COLORTABLEPROPERTY(White);
        GETSET_COLORTABLEPROPERTY(BrightBlack);
        GETSET_COLORTABLEPROPERTY(BrightRed);
        GETSET_COLORTABLEPROPERTY(BrightGreen);
        GETSET_COLORTABLEPROPERTY(BrightYellow);
        GETSET_COLORTABLEPROPERTY(BrightBlue);
        GETSET_COLORTABLEPROPERTY(BrightPurple);
        GETSET_COLORTABLEPROPERTY(BrightCyan);
        GETSET_COLORTABLEPROPERTY(BrightWhite);

    private:
        std::array<til::color, COLOR_TABLE_SIZE> _table;

        static constexpr unsigned int BlackIndex{ 0 };
        static constexpr unsigned int RedIndex{ 1 };
        static constexpr unsigned int GreenIndex{ 2 };
        static constexpr unsigned int YellowIndex{ 3 };
        static constexpr unsigned int BlueIndex{ 4 };
        static constexpr unsigned int PurpleIndex{ 5 };
        static constexpr unsigned int CyanIndex{ 6 };
        static constexpr unsigned int WhiteIndex{ 7 };
        static constexpr unsigned int BrightBlackIndex{ 8 };
        static constexpr unsigned int BrightRedIndex{ 9 };
        static constexpr unsigned int BrightGreenIndex{ 10 };
        static constexpr unsigned int BrightYellowIndex{ 11 };
        static constexpr unsigned int BrightBlueIndex{ 12 };
        static constexpr unsigned int BrightPurpleIndex{ 13 };
        static constexpr unsigned int BrightCyanIndex{ 14 };
        static constexpr unsigned int BrightWhiteIndex{ 15 };

        friend class TerminalAppLocalTests::SettingsTests;
        friend class TerminalAppLocalTests::ColorSchemeTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ColorScheme);
}
