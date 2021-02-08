// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"
//#include "../../types/colorTable.cpp"

#include "TerminalSettings.g.cpp"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    static constexpr std::array<til::color, 16> campbellColorTable{
        til::color{ 0x0C, 0x0C, 0x0C },
        til::color{ 0xC5, 0x0F, 0x1F },
        til::color{ 0x13, 0xA1, 0x0E },
        til::color{ 0xC1, 0x9C, 0x00 },
        til::color{ 0x00, 0x37, 0xDA },
        til::color{ 0x88, 0x17, 0x98 },
        til::color{ 0x3A, 0x96, 0xDD },
        til::color{ 0xCC, 0xCC, 0xCC },
        til::color{ 0x76, 0x76, 0x76 },
        til::color{ 0xE7, 0x48, 0x56 },
        til::color{ 0x16, 0xC6, 0x0C },
        til::color{ 0xF9, 0xF1, 0xA5 },
        til::color{ 0x3B, 0x78, 0xFF },
        til::color{ 0xB4, 0x00, 0x9E },
        til::color{ 0x61, 0xD6, 0xD6 },
        til::color{ 0xF2, 0xF2, 0xF2 },
    };

    static std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment> ConvertConvergedAlignment(ConvergedAlignment alignment)
    {
        // extract horizontal alignment
        Windows::UI::Xaml::HorizontalAlignment horizAlign;
        switch (alignment & static_cast<ConvergedAlignment>(0x0F))
        {
        case ConvergedAlignment::Horizontal_Left:
            horizAlign = Windows::UI::Xaml::HorizontalAlignment::Left;
            break;
        case ConvergedAlignment::Horizontal_Right:
            horizAlign = Windows::UI::Xaml::HorizontalAlignment::Right;
            break;
        case ConvergedAlignment::Horizontal_Center:
        default:
            horizAlign = Windows::UI::Xaml::HorizontalAlignment::Center;
            break;
        }

        // extract vertical alignment
        Windows::UI::Xaml::VerticalAlignment vertAlign;
        switch (alignment & static_cast<ConvergedAlignment>(0xF0))
        {
        case ConvergedAlignment::Vertical_Top:
            vertAlign = Windows::UI::Xaml::VerticalAlignment::Top;
            break;
        case ConvergedAlignment::Vertical_Bottom:
            vertAlign = Windows::UI::Xaml::VerticalAlignment::Bottom;
            break;
        case ConvergedAlignment::Vertical_Center:
        default:
            vertAlign = Windows::UI::Xaml::VerticalAlignment::Center;
            break;
        }

        return { horizAlign, vertAlign };
    }

    TerminalSettings::TerminalSettings(const CascadiaSettings& appSettings, winrt::guid profileGuid, const IKeyBindings& keybindings) :
        _KeyBindings{ keybindings }
    {
        const auto profile = appSettings.FindProfile(profileGuid);
        THROW_HR_IF_NULL(E_INVALIDARG, profile);

        const auto globals = appSettings.GlobalSettings();
        _ApplyProfileSettings(profile, globals.ColorSchemes());
        _ApplyGlobalSettings(globals);
    }

    // Method Description:
    // - Create a TerminalSettings object for the provided newTerminalArgs. We'll
    //   use the newTerminalArgs to look up the profile that should be used to
    //   create these TerminalSettings. Then, we'll apply settings contained in the
    //   newTerminalArgs to the profile's settings, to enable customization on top
    //   of the profile's default values.
    // Arguments:
    // - appSettings: the set of settings being used to construct the new terminal
    // - newTerminalArgs: An object that may contain a profile name or GUID to
    //   actually use. If the Profile value is not a guid, we'll treat it as a name,
    //   and attempt to look the profile up by name instead.
    //   * Additionally, we'll use other values (such as Commandline,
    //     StartingDirectory) in this object to override the settings directly from
    //     the profile.
    // - keybindings: the keybinding handler
    // Return Value:
    // - the GUID of the created profile, and a fully initialized TerminalSettings object
    std::tuple<guid, TerminalApp::TerminalSettings> TerminalSettings::BuildSettings(const CascadiaSettings& appSettings,
                                                                                    const NewTerminalArgs& newTerminalArgs,
                                                                                    const IKeyBindings& keybindings)
    {
        const guid profileGuid = appSettings.GetProfileForArgs(newTerminalArgs);
        auto settings{ winrt::make<TerminalSettings>(appSettings, profileGuid, keybindings) };

        if (newTerminalArgs)
        {
            // Override commandline, starting directory if they exist in newTerminalArgs
            if (!newTerminalArgs.Commandline().empty())
            {
                settings.Commandline(newTerminalArgs.Commandline());
            }
            if (!newTerminalArgs.StartingDirectory().empty())
            {
                settings.StartingDirectory(newTerminalArgs.StartingDirectory());
            }
            if (!newTerminalArgs.TabTitle().empty())
            {
                settings.StartingTitle(newTerminalArgs.TabTitle());
            }
            if (newTerminalArgs.TabColor())
            {
                settings.StartingTabColor(static_cast<uint32_t>(til::color(newTerminalArgs.TabColor().Value())));
            }
        }

        return { profileGuid, settings };
    }

    // Method Description:
    // - Apply Profile settings, as well as any colors from our color scheme, if we have one.
    // Arguments:
    // - profile: the profile settings we're applying
    // - schemes: a map of schemes to look for our color scheme in, if we have one.
    // Return Value:
    // - <none>
    void TerminalSettings::_ApplyProfileSettings(const Profile& profile, const Windows::Foundation::Collections::IMapView<winrt::hstring, ColorScheme>& schemes)
    {
        // Fill in the Terminal Setting's CoreSettings from the profile
        _HistorySize = profile.HistorySize();
        _SnapOnInput = profile.SnapOnInput();
        _AltGrAliasing = profile.AltGrAliasing();
        _CursorHeight = profile.CursorHeight();
        _CursorShape = profile.CursorShape();

        // Fill in the remaining properties from the profile
        _ProfileName = profile.Name();
        _UseAcrylic = profile.UseAcrylic();
        _TintOpacity = profile.AcrylicOpacity();

        _FontFace = profile.FontFace();
        _FontSize = profile.FontSize();
        _FontWeight = profile.FontWeight();
        _Padding = profile.Padding();

        _Commandline = profile.Commandline();

        _StartingDirectory = profile.EvaluatedStartingDirectory();

        // GH#2373: Use the tabTitle as the starting title if it exists, otherwise
        // use the profile name
        _StartingTitle = !profile.TabTitle().empty() ? profile.TabTitle() : profile.Name();

        if (profile.SuppressApplicationTitle())
        {
            _SuppressApplicationTitle = profile.SuppressApplicationTitle();
        }

        if (!profile.ColorSchemeName().empty())
        {
            if (const auto scheme = schemes.TryLookup(profile.ColorSchemeName()))
            {
                ApplyColorScheme(scheme);
            }
        }
        if (profile.Foreground())
        {
            _DefaultForeground = til::color{ profile.Foreground().Value() };
        }
        if (profile.Background())
        {
            _DefaultBackground = til::color{ profile.Background().Value() };
        }
        if (profile.SelectionBackground())
        {
            _SelectionBackground = til::color{ profile.SelectionBackground().Value() };
        }
        if (profile.CursorColor())
        {
            _CursorColor = til::color{ profile.CursorColor().Value() };
        }

        _ScrollState = profile.ScrollState();

        if (!profile.BackgroundImagePath().empty())
        {
            _BackgroundImage = profile.ExpandedBackgroundImagePath();
        }

        _BackgroundImageOpacity = profile.BackgroundImageOpacity();
        _BackgroundImageStretchMode = profile.BackgroundImageStretchMode();
        std::tie(_BackgroundImageHorizontalAlignment, _BackgroundImageVerticalAlignment) = ConvertConvergedAlignment(profile.BackgroundImageAlignment());

        _RetroTerminalEffect = profile.RetroTerminalEffect();
        _PixelShaderPath = winrt::hstring{ wil::ExpandEnvironmentStringsW<std::wstring>(profile.PixelShaderPath().c_str()) };

        _AntialiasingMode = profile.AntialiasingMode();

        if (profile.TabColor())
        {
            const til::color colorRef{ profile.TabColor().Value() };
            _TabColor = static_cast<uint32_t>(colorRef);
        }
    }

    // Method Description:
    // - Applies appropriate settings from the globals into the TerminalSettings object.
    // Arguments:
    // - globalSettings: the global property values we're applying.
    // Return Value:
    // - <none>
    void TerminalSettings::_ApplyGlobalSettings(const GlobalAppSettings& globalSettings) noexcept
    {
        _InitialRows = globalSettings.InitialRows();
        _InitialCols = globalSettings.InitialCols();

        _WordDelimiters = globalSettings.WordDelimiters();
        _CopyOnSelect = globalSettings.CopyOnSelect();
        _ForceFullRepaintRendering = globalSettings.ForceFullRepaintRendering();
        _SoftwareRendering = globalSettings.SoftwareRendering();
        _ForceVTInput = globalSettings.ForceVTInput();
    }

    // Method Description:
    // - Apply a given ColorScheme's values to the TerminalSettings object.
    //      Sets the foreground, background, and color table of the settings object.
    // Arguments:
    // - scheme: the ColorScheme we are applying to the TerminalSettings object
    // Return Value:
    // - <none>
    void TerminalSettings::ApplyColorScheme(const ColorScheme& scheme)
    {
        _DefaultForeground = til::color{ scheme.Foreground() };
        _DefaultBackground = til::color{ scheme.Background() };
        _SelectionBackground = til::color{ scheme.SelectionBackground() };
        _CursorColor = til::color{ scheme.CursorColor() };

        const auto table = scheme.Table();
        std::array<uint32_t, COLOR_TABLE_SIZE> colorTable{};
        std::transform(table.cbegin(), table.cend(), colorTable.begin(), [](auto&& color) {
            return static_cast<uint32_t>(til::color{ color });
        });
        ColorTable(colorTable);
    }

    uint32_t TerminalSettings::GetColorTableEntry(int32_t index) noexcept
    {
        return ColorTable().at(index);
    }

    void TerminalSettings::ColorTable(std::array<uint32_t, 16> colors)
    {
        _ColorTable = colors;
    }

    std::array<uint32_t, COLOR_TABLE_SIZE> TerminalSettings::ColorTable()
    {
        auto span = _getColorTableImpl();
        std::array<uint32_t, COLOR_TABLE_SIZE> colorTable{};
        if (span.size() > 0)
        {
            std::transform(span.begin(), span.end(), colorTable.begin(), [](auto&& color) {
                return static_cast<uint32_t>(til::color{ color });
            });
        }
        else
        {
            std::transform(campbellColorTable.cbegin(), campbellColorTable.cend(), colorTable.begin(), [](auto&& color) {
                return static_cast<uint32_t>(til::color{ color });
            });
        }
        return colorTable;
        //return campbellColorTable;
        // issues:
        // - converting span to array without brute force?
        // - converting from array<color> to array<uint32> without brute force?
        // - including colortable.cpp for the campbell color table causes errors
    }

    gsl::span<uint32_t> TerminalSettings::_getColorTableImpl()
    {
        if (_ColorTable.has_value())
        {
            return gsl::make_span(*_ColorTable);
        }
        for (auto&& parent : _parents)
        {
            auto parentSpan = parent->_getColorTableImpl();
            if (parentSpan.size() > 0)
            {
                return parentSpan;
            }
        }
        return {};
    }
}
