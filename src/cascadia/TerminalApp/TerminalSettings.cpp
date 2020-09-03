// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"

#include "TerminalSettings.g.cpp"

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Create a TerminalSettings object for the provided newTerminalArgs. We'll
    //   use the newTerminalArgs to look up the profile that should be used to
    //   create these TerminalSettings. Then, we'll apply settings contained in the
    //   newTerminalArgs to the profile's settings, to enable customization on top
    //   of the profile's default values.
    // Arguments:
    // - newTerminalArgs: An object that may contain a profile name or GUID to
    //   actually use. If the Profile value is not a guid, we'll treat it as a name,
    //   and attempt to look the profile up by name instead.
    //   * Additionally, we'll use other values (such as Commandline,
    //     StartingDirectory) in this object to override the settings directly from
    //     the profile.
    // Return Value:
    // - the GUID of the created profile, and a fully initialized TerminalSettings object
    std::tuple<winrt::guid, winrt::TerminalApp::TerminalSettings> TerminalSettings::BuildSettings(const ::TerminalApp::CascadiaSettings& appSettings, const winrt::TerminalApp::NewTerminalArgs& newTerminalArgs)
    {
        const winrt::guid profileGuid = appSettings.GetProfileForArgs(newTerminalArgs);
        auto settings = BuildSettings(appSettings, profileGuid);

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
    // - Create a TerminalSettings object for the profile with a GUID matching the
    //   provided GUID. If no profile matches this GUID, then this method will
    //   throw.
    // Arguments:
    // - profileGuid: The GUID of a profile to use to create a settings object for.
    // Return Value:
    // - a fully initialized TerminalSettings object
    winrt::TerminalApp::TerminalSettings TerminalSettings::BuildSettings(const ::TerminalApp::CascadiaSettings& appSettings, winrt::guid profileGuid)
    {
        const auto profile = appSettings.FindProfile(profileGuid);
        THROW_HR_IF_NULL(E_INVALIDARG, profile);

        const auto globals = appSettings.GlobalSettings();
        TerminalApp::TerminalSettings result = _CreateTerminalSettings(profile, globals.GetColorSchemes());

        // Place our appropriate global settings into the Terminal Settings
        _ApplyToSettings(globals, result);

        return result;
    }

    // Method Description:
    // - Create a TerminalSettings from this object. Apply our settings, as well as
    //      any colors from our color scheme, if we have one.
    // Arguments:
    // - schemes: a list of schemes to look for our color scheme in, if we have one.
    // Return Value:
    // - a new TerminalSettings object with our settings in it.
    winrt::TerminalApp::TerminalSettings TerminalSettings::_CreateTerminalSettings(const TerminalApp::Profile& profile, const winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, TerminalApp::ColorScheme>& schemes)
    {
        auto terminalSettings = winrt::make<TerminalSettings>();

        // Fill in the Terminal Setting's CoreSettings from the profile
        terminalSettings.HistorySize(profile.HistorySize());
        terminalSettings.SnapOnInput(profile.SnapOnInput());
        terminalSettings.AltGrAliasing(profile.AltGrAliasing());
        terminalSettings.CursorHeight(profile.CursorHeight());
        terminalSettings.CursorShape(profile.CursorShape());

        // Fill in the remaining properties from the profile
        terminalSettings.ProfileName(profile.Name());
        terminalSettings.UseAcrylic(profile.UseAcrylic());
        terminalSettings.TintOpacity(profile.AcrylicOpacity());

        terminalSettings.FontFace(profile.FontFace());
        terminalSettings.FontSize(profile.FontSize());
        terminalSettings.FontWeight(profile.FontWeight());
        terminalSettings.Padding(profile.Padding());

        terminalSettings.Commandline(profile.Commandline());

        if (!profile.StartingDirectory().empty())
        {
            terminalSettings.StartingDirectory(profile.GetEvaluatedStartingDirectory());
        }

        // GH#2373: Use the tabTitle as the starting title if it exists, otherwise
        // use the profile name
        terminalSettings.StartingTitle(!profile.TabTitle().empty() ? profile.TabTitle() : profile.Name());

        if (profile.SuppressApplicationTitle())
        {
            terminalSettings.SuppressApplicationTitle(profile.SuppressApplicationTitle());
        }

        if (!profile.ColorSchemeName().empty())
        {
            if (const auto found{ schemes.TryLookup(profile.ColorSchemeName()) })
            {
                found.ApplyScheme(terminalSettings);
            }
        }
        if (profile.Foreground())
        {
            const til::color colorRef{ profile.Foreground().Value() };
            terminalSettings.DefaultForeground(static_cast<uint32_t>(colorRef));
        }
        if (profile.Background())
        {
            const til::color colorRef{ profile.Background().Value() };
            terminalSettings.DefaultBackground(static_cast<uint32_t>(colorRef));
        }
        if (profile.SelectionBackground())
        {
            const til::color colorRef{ profile.SelectionBackground().Value() };
            terminalSettings.SelectionBackground(static_cast<uint32_t>(colorRef));
        }
        if (profile.CursorColor())
        {
            const til::color colorRef{ profile.CursorColor().Value() };
            terminalSettings.CursorColor(static_cast<uint32_t>(colorRef));
        }

        terminalSettings.ScrollState(profile.ScrollState());

        if (!profile.BackgroundImagePath().empty())
        {
            terminalSettings.BackgroundImage(profile.GetExpandedBackgroundImagePath());
        }

        terminalSettings.BackgroundImageOpacity(profile.BackgroundImageOpacity());
        terminalSettings.BackgroundImageStretchMode(profile.BackgroundImageStretchMode());

        terminalSettings.BackgroundImageHorizontalAlignment(profile.BackgroundImageHorizontalAlignment());
        terminalSettings.BackgroundImageVerticalAlignment(profile.BackgroundImageVerticalAlignment());

        terminalSettings.RetroTerminalEffect(profile.RetroTerminalEffect());

        terminalSettings.AntialiasingMode(profile.AntialiasingMode());

        if (profile.TabColor())
        {
            const til::color colorRef{ profile.TabColor().Value() };
            terminalSettings.TabColor(static_cast<uint32_t>(colorRef));
        }

        return terminalSettings;
    }

    // Method Description:
    // - Applies appropriate settings from the globals into the given TerminalSettings.
    // Arguments:
    // - settings: a TerminalSettings object to add global property values to.
    // Return Value:
    // - <none>
    void TerminalSettings::_ApplyToSettings(const TerminalApp::GlobalAppSettings& globalSettings, const TerminalApp::TerminalSettings& settings) noexcept
    {
        settings.InitialRows(globalSettings.InitialRows());
        settings.InitialCols(globalSettings.InitialCols());

        settings.WordDelimiters(globalSettings.WordDelimiters());
        settings.CopyOnSelect(globalSettings.CopyOnSelect());
        settings.ForceFullRepaintRendering(globalSettings.ForceFullRepaintRendering());
        settings.SoftwareRendering(globalSettings.SoftwareRendering());
        settings.ForceVTInput(globalSettings.ForceVTInput());
    }

    uint32_t TerminalSettings::GetColorTableEntry(int32_t index) const noexcept
    {
        return _colorTable.at(index);
    }

    void TerminalSettings::SetColorTableEntry(int32_t index, uint32_t value)
    {
        auto const colorTableCount = gsl::narrow_cast<decltype(index)>(_colorTable.size());
        THROW_HR_IF(E_INVALIDARG, index >= colorTableCount);
        _colorTable.at(index) = value;
    }
}
