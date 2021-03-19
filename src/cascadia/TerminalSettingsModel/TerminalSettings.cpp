// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"
#include "../../types/inc/colorTable.hpp"

#include "TerminalSettings.g.cpp"

using namespace winrt::Microsoft::Terminal::Control;
using namespace Microsoft::Console::Utils;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
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

    // Method Description:
    // - Create a TerminalSettings object for the provided profile guid. We'll
    //   use the guid to look up the profile that should be used to
    //   create these TerminalSettings. Then, we'll apply settings contained in the
    //   global and profile settings to the instance.
    // Arguments:
    // - appSettings: the set of settings being used to construct the new terminal
    // - profileGuid: the unique identifier (guid) of the profile
    // - keybindings: the keybinding handler
    Model::TerminalSettingsStruct TerminalSettings::CreateWithProfileByID(const Model::CascadiaSettings& appSettings, winrt::guid profileGuid, const IKeyBindings& keybindings)
    {
        auto settings{ winrt::make_self<TerminalSettings>() };
        settings->_KeyBindings = keybindings;

        const auto profile = appSettings.FindProfile(profileGuid);
        THROW_HR_IF_NULL(E_INVALIDARG, profile);

        const auto globals = appSettings.GlobalSettings();
        settings->_ApplyProfileSettings(profile);
        settings->_ApplyGlobalSettings(globals);
        settings->ApplyAppearanceSettings(profile.DefaultAppearance(), globals.ColorSchemes());

        if (profile.UnfocusedAppearance())
        {
            auto child = settings->CreateChild();
            child->ApplyAppearanceSettings(profile.UnfocusedAppearance(), globals.ColorSchemes());
            return winrt::make<TerminalSettingsStruct>(*settings, *child);
        }

        return winrt::make<TerminalSettingsStruct>(*settings, nullptr);
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
    Model::TerminalSettingsStruct TerminalSettings::CreateWithNewTerminalArgs(const CascadiaSettings& appSettings,
                                                                              const NewTerminalArgs& newTerminalArgs,
                                                                              const IKeyBindings& keybindings)
    {
        const guid profileGuid = appSettings.GetProfileForArgs(newTerminalArgs);
        auto settings = CreateWithProfileByID(appSettings, profileGuid, keybindings);

        if (newTerminalArgs)
        {
            // Override commandline, starting directory if they exist in newTerminalArgs
            if (!newTerminalArgs.Commandline().empty())
            {
                settings.DefaultSettings().Commandline(newTerminalArgs.Commandline());
            }
            if (!newTerminalArgs.StartingDirectory().empty())
            {
                settings.DefaultSettings().StartingDirectory(newTerminalArgs.StartingDirectory());
            }
            if (!newTerminalArgs.TabTitle().empty())
            {
                settings.DefaultSettings().StartingTitle(newTerminalArgs.TabTitle());
            }
            if (newTerminalArgs.TabColor())
            {
                settings.DefaultSettings().StartingTabColor(static_cast<uint32_t>(til::color(newTerminalArgs.TabColor().Value())));
            }
            if (newTerminalArgs.SuppressApplicationTitle())
            {
                settings.DefaultSettings().SuppressApplicationTitle(newTerminalArgs.SuppressApplicationTitle().Value());
            }
        }

        return settings;
    }

    void TerminalSettings::ApplyAppearanceSettings(const IAppearanceConfig& appearance, const Windows::Foundation::Collections::IMapView<winrt::hstring, ColorScheme>& schemes)
    {
        _CursorShape = appearance.CursorShape();
        _CursorHeight = appearance.CursorHeight();
        if (!appearance.ColorSchemeName().empty())
        {
            if (const auto scheme = schemes.TryLookup(appearance.ColorSchemeName()))
            {
                ApplyColorScheme(scheme);
            }
        }
        if (appearance.Foreground())
        {
            _DefaultForeground = til::color{ appearance.Foreground().Value() };
        }
        if (appearance.Background())
        {
            _DefaultBackground = til::color{ appearance.Background().Value() };
        }
        if (appearance.SelectionBackground())
        {
            _SelectionBackground = til::color{ appearance.SelectionBackground().Value() };
        }
        if (appearance.CursorColor())
        {
            _CursorColor = til::color{ appearance.CursorColor().Value() };
        }
        if (!appearance.BackgroundImagePath().empty())
        {
            _BackgroundImage = appearance.ExpandedBackgroundImagePath();
        }

        _BackgroundImageOpacity = appearance.BackgroundImageOpacity();
        _BackgroundImageStretchMode = appearance.BackgroundImageStretchMode();
        std::tie(_BackgroundImageHorizontalAlignment, _BackgroundImageVerticalAlignment) = ConvertConvergedAlignment(appearance.BackgroundImageAlignment());

        _RetroTerminalEffect = appearance.RetroTerminalEffect();
        _PixelShaderPath = winrt::hstring{ wil::ExpandEnvironmentStringsW<std::wstring>(appearance.PixelShaderPath().c_str()) };
    }

    // Method Description:
    // - Creates a TerminalSettings object that inherits from a parent TerminalSettings
    // Arguments::
    // - parent: the TerminalSettings object that the newly created TerminalSettings will inherit from
    // Return Value:
    // - a newly created child of the given parent object
    Model::TerminalSettingsStruct TerminalSettings::CreateWithParent(const Model::TerminalSettingsStruct& parent)
    {
        THROW_HR_IF_NULL(E_INVALIDARG, parent);

        auto defaultImpl{ get_self<TerminalSettings>(parent.DefaultSettings()) };
        auto defaultChild = defaultImpl->CreateChild();
        if (parent.UnfocusedSettings())
        {
            parent.UnfocusedSettings().SetParent(*defaultChild);
        }
        return winrt::make<TerminalSettingsStruct>(*defaultChild, parent.UnfocusedSettings());
    }

    // Method Description:
    // - Sets our parent to the provided TerminalSettings
    // Arguments:
    // - parent: our new parent
    void TerminalSettings::SetParent(const Model::TerminalSettings& parent)
    {
        ClearParents();
        com_ptr<TerminalSettings> parentImpl;
        parentImpl.copy_from(get_self<TerminalSettings>(parent));
        InsertParent(parentImpl);
    }

    // Method Description:
    // - Apply Profile settings, as well as any colors from our color scheme, if we have one.
    // Arguments:
    // - profile: the profile settings we're applying
    // - schemes: a map of schemes to look for our color scheme in, if we have one.
    // Return Value:
    // - <none>
    void TerminalSettings::_ApplyProfileSettings(const Profile& profile)
    {
        // Fill in the Terminal Setting's CoreSettings from the profile
        _HistorySize = profile.HistorySize();
        _SnapOnInput = profile.SnapOnInput();
        _AltGrAliasing = profile.AltGrAliasing();

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

        _ScrollState = profile.ScrollState();

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
    void TerminalSettings::_ApplyGlobalSettings(const Model::GlobalAppSettings& globalSettings) noexcept
    {
        _InitialRows = globalSettings.InitialRows();
        _InitialCols = globalSettings.InitialCols();

        _WordDelimiters = globalSettings.WordDelimiters();
        _CopyOnSelect = globalSettings.CopyOnSelect();
        _FocusFollowMouse = globalSettings.FocusFollowMouse();
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
    void TerminalSettings::ApplyColorScheme(const Model::ColorScheme& scheme)
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
            const auto campbellSpan = CampbellColorTable();
            std::transform(campbellSpan.begin(), campbellSpan.end(), colorTable.begin(), [](auto&& color) {
                return static_cast<uint32_t>(til::color{ color });
            });
        }
        return colorTable;
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
