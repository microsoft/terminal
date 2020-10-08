// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"

#include "TerminalSettings.g.cpp"

using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
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

        if (!profile.StartingDirectory().empty())
        {
            _StartingDirectory = profile.EvaluatedStartingDirectory();
        }

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

        _BackgroundImageHorizontalAlignment = profile.BackgroundImageHorizontalAlignment();
        _BackgroundImageVerticalAlignment = profile.BackgroundImageVerticalAlignment();

        _RetroTerminalEffect = profile.RetroTerminalEffect();

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
        std::transform(table.cbegin(), table.cend(), _colorTable.begin(), [](auto&& color) {
            return static_cast<uint32_t>(til::color{ color });
        });
    }

    uint32_t TerminalSettings::GetColorTableEntry(int32_t index) const noexcept
    {
        return _colorTable.at(index);
    }
}
