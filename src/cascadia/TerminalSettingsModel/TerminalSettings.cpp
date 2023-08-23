// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"
#include "../../types/inc/colorTable.hpp"

#include "TerminalSettings.g.cpp"
#include "TerminalSettingsCreateResult.g.cpp"

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

    winrt::com_ptr<implementation::TerminalSettings> TerminalSettings::_CreateWithProfileCommon(const Model::CascadiaSettings& appSettings, const Model::Profile& profile)
    {
        auto settings{ winrt::make_self<TerminalSettings>() };

        const auto globals = appSettings.GlobalSettings();
        settings->_ApplyProfileSettings(profile);
        settings->_ApplyGlobalSettings(globals);
        settings->_ApplyAppearanceSettings(profile.DefaultAppearance(), globals.ColorSchemes(), globals.CurrentTheme());

        return settings;
    }

    Model::TerminalSettings TerminalSettings::CreateForPreview(const Model::CascadiaSettings& appSettings, const Model::Profile& profile)
    {
        const auto settings = _CreateWithProfileCommon(appSettings, profile);
        settings->UseBackgroundImageForWindow(false);
        return *settings;
    }

    // Method Description:
    // - Create a TerminalSettingsCreateResult for the provided profile guid. We'll
    //   use the guid to look up the profile that should be used to
    //   create these TerminalSettings. Then, we'll apply settings contained in the
    //   global and profile settings to the instance.
    // Arguments:
    // - appSettings: the set of settings being used to construct the new terminal
    // - profileGuid: the unique identifier (guid) of the profile
    // - keybindings: the keybinding handler
    // Return Value:
    // - A TerminalSettingsCreateResult, which contains a pair of TerminalSettings objects,
    //   one for when the terminal is focused and the other for when the terminal is unfocused
    Model::TerminalSettingsCreateResult TerminalSettings::CreateWithProfile(const Model::CascadiaSettings& appSettings, const Model::Profile& profile, const IKeyBindings& keybindings)
    {
        const auto settings = _CreateWithProfileCommon(appSettings, profile);
        settings->_KeyBindings = keybindings;

        Model::TerminalSettings child{ nullptr };
        if (const auto& unfocusedAppearance{ profile.UnfocusedAppearance() })
        {
            const auto globals = appSettings.GlobalSettings();
            auto childImpl = settings->CreateChild();
            childImpl->_ApplyAppearanceSettings(unfocusedAppearance, globals.ColorSchemes(), globals.CurrentTheme());
            child = *childImpl;
        }

        return winrt::make<TerminalSettingsCreateResult>(*settings, child);
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
    // - A TerminalSettingsCreateResult object, which contains a pair of TerminalSettings
    //   objects. One for when the terminal is focused and one for when the terminal is unfocused.
    Model::TerminalSettingsCreateResult TerminalSettings::CreateWithNewTerminalArgs(const CascadiaSettings& appSettings,
                                                                                    const NewTerminalArgs& newTerminalArgs,
                                                                                    const IKeyBindings& keybindings)
    {
        const auto profile = appSettings.GetProfileForArgs(newTerminalArgs);
        auto settingsPair{ CreateWithProfile(appSettings, profile, keybindings) };
        auto defaultSettings = settingsPair.DefaultSettings();

        if (newTerminalArgs)
        {
            // Override commandline, starting directory if they exist in newTerminalArgs
            if (!newTerminalArgs.Commandline().empty())
            {
                defaultSettings.Commandline(newTerminalArgs.Commandline());
            }
            if (!newTerminalArgs.StartingDirectory().empty())
            {
                defaultSettings.StartingDirectory(newTerminalArgs.StartingDirectory());
            }
            if (!newTerminalArgs.TabTitle().empty())
            {
                defaultSettings.StartingTitle(newTerminalArgs.TabTitle());
            }
            else
            {
                // There was no title, and no profile from which to infer the title.
                // Per GH#6776, promote the first component of the command line to the title.
                // This will ensure that the tab we spawn has a name (since it didn't get one from its profile!)
                if (newTerminalArgs.Profile().empty() && !newTerminalArgs.Commandline().empty())
                {
                    const std::wstring_view commandLine{ newTerminalArgs.Commandline() };
                    const auto start{ til::at(commandLine, 0) == L'"' ? 1 : 0 };
                    const auto terminator{ commandLine.find_first_of(start ? L'"' : L' ', start) }; // look past the first character if it starts with "
                    // We have to take a copy here; winrt::param::hstring requires a null-terminated string
                    const std::wstring firstComponent{ commandLine.substr(start, terminator - start) };
                    defaultSettings.StartingTitle(firstComponent);
                }
            }
            if (newTerminalArgs.TabColor())
            {
                defaultSettings.StartingTabColor(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>{ static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ newTerminalArgs.TabColor().Value() }) });
            }
            if (newTerminalArgs.SuppressApplicationTitle())
            {
                defaultSettings.SuppressApplicationTitle(newTerminalArgs.SuppressApplicationTitle().Value());
            }
            if (!newTerminalArgs.ColorScheme().empty())
            {
                const auto schemes = appSettings.GlobalSettings().ColorSchemes();
                if (const auto& scheme = schemes.TryLookup(newTerminalArgs.ColorScheme()))
                {
                    defaultSettings.ApplyColorScheme(scheme);
                }
            }
            // Elevate on NewTerminalArgs is an optional value, so the default
            // value (null) doesn't override a profile's value. Note that
            // elevate:false in an already elevated terminal does nothing - the
            // profile will still be launched elevated.
            if (newTerminalArgs.Elevate())
            {
                defaultSettings.Elevate(newTerminalArgs.Elevate().Value());
            }
        }

        return settingsPair;
    }

    void TerminalSettings::_ApplyAppearanceSettings(const IAppearanceConfig& appearance,
                                                    const Windows::Foundation::Collections::IMapView<winrt::hstring, ColorScheme>& schemes,
                                                    const winrt::Microsoft::Terminal::Settings::Model::Theme currentTheme)
    {
        _CursorShape = appearance.CursorShape();
        _CursorHeight = appearance.CursorHeight();

        auto requestedTheme = currentTheme.RequestedTheme();
        if (requestedTheme == winrt::Windows::UI::Xaml::ElementTheme::Default)
        {
            requestedTheme = Model::Theme::IsSystemInDarkTheme() ?
                                 winrt::Windows::UI::Xaml::ElementTheme::Dark :
                                 winrt::Windows::UI::Xaml::ElementTheme::Light;
        }

        switch (requestedTheme)
        {
        case winrt::Windows::UI::Xaml::ElementTheme::Light:
            if (const auto scheme = schemes.TryLookup(appearance.LightColorSchemeName()))
            {
                ApplyColorScheme(scheme);
            }
            break;
        case winrt::Windows::UI::Xaml::ElementTheme::Dark:
            if (const auto scheme = schemes.TryLookup(appearance.DarkColorSchemeName()))
            {
                ApplyColorScheme(scheme);
            }
            break;
        case winrt::Windows::UI::Xaml::ElementTheme::Default:
            // This shouldn't happen!
            break;
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

        _IntenseIsBold = WI_IsFlagSet(appearance.IntenseTextStyle(), Microsoft::Terminal::Settings::Model::IntenseStyle::Bold);
        _IntenseIsBright = WI_IsFlagSet(appearance.IntenseTextStyle(), Microsoft::Terminal::Settings::Model::IntenseStyle::Bright);

        _AdjustIndistinguishableColors = appearance.AdjustIndistinguishableColors();
        _Opacity = appearance.Opacity();
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
        _ProfileSource = profile.Source();
        _UseAcrylic = profile.UseAcrylic();

        const auto fontInfo = profile.FontInfo();
        _FontFace = fontInfo.FontFace();
        _FontSize = fontInfo.FontSize();
        _FontWeight = fontInfo.FontWeight();
        _FontFeatures = fontInfo.FontFeatures();
        _FontAxes = fontInfo.FontAxes();
        _CellWidth = fontInfo.CellWidth();
        _CellHeight = fontInfo.CellHeight();
        _Padding = profile.Padding();

        _Commandline = profile.Commandline();
        _VtPassthrough = profile.VtPassthrough();

        _StartingDirectory = profile.EvaluatedStartingDirectory();

        // GH#2373: Use the tabTitle as the starting title if it exists, otherwise
        // use the profile name
        _StartingTitle = !profile.TabTitle().empty() ? profile.TabTitle() : profile.Name();

        if (profile.SuppressApplicationTitle())
        {
            _SuppressApplicationTitle = profile.SuppressApplicationTitle();
        }

        _UseAtlasEngine = profile.UseAtlasEngine();
        _ScrollState = profile.ScrollState();

        _AntialiasingMode = profile.AntialiasingMode();

        if (profile.TabColor())
        {
            const til::color colorRef{ profile.TabColor().Value() };
            _TabColor = static_cast<winrt::Microsoft::Terminal::Core::Color>(colorRef);
        }

        const auto profileEnvVars = profile.EnvironmentVariables();
        if (profileEnvVars == nullptr)
        {
            _EnvironmentVariables = std::nullopt;
        }
        else
        {
            _EnvironmentVariables = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
            for (const auto& [key, value] : profileEnvVars)
            {
                _EnvironmentVariables.value().Insert(key, value);
            }
        }

        _Elevate = profile.Elevate();
        _AutoMarkPrompts = Feature_ScrollbarMarks::IsEnabled() && profile.AutoMarkPrompts();
        _ShowMarks = Feature_ScrollbarMarks::IsEnabled() && profile.ShowMarks();

        _RightClickContextMenu = profile.RightClickContextMenu();

        _RepositionCursorWithMouse = profile.RepositionCursorWithMouse();
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
        _UseBackgroundImageForWindow = globalSettings.UseBackgroundImageForWindow();
        _ForceVTInput = globalSettings.ForceVTInput();
        _TrimBlockSelection = globalSettings.TrimBlockSelection();
        _DetectURLs = globalSettings.DetectURLs();
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
        // If the scheme was nullptr, then just clear out the current color
        // settings.
        if (scheme == nullptr)
        {
            ClearAppliedColorScheme();
            ClearDefaultForeground();
            ClearDefaultBackground();
            ClearSelectionBackground();
            ClearCursorColor();
            _ColorTable = std::nullopt;
        }
        else
        {
            AppliedColorScheme(scheme);
            _DefaultForeground = til::color{ scheme.Foreground() };
            _DefaultBackground = til::color{ scheme.Background() };
            _SelectionBackground = til::color{ scheme.SelectionBackground() };
            _CursorColor = til::color{ scheme.CursorColor() };

            const auto table = scheme.Table();
            std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> colorTable{};
            std::transform(table.cbegin(), table.cend(), colorTable.begin(), [](auto&& color) {
                return static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ color });
            });
            ColorTable(colorTable);
        }
    }

    winrt::Microsoft::Terminal::Core::Color TerminalSettings::GetColorTableEntry(int32_t index) noexcept
    {
        return ColorTable().at(index);
    }

    void TerminalSettings::ColorTable(std::array<winrt::Microsoft::Terminal::Core::Color, 16> colors)
    {
        _ColorTable = colors;
    }

    std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> TerminalSettings::ColorTable()
    {
        auto span = _getColorTableImpl();
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> colorTable{};
        if (span.size() > 0)
        {
            std::copy(span.begin(), span.end(), colorTable.begin());
        }
        else
        {
            const auto campbellSpan = CampbellColorTable();
            std::transform(campbellSpan.begin(), campbellSpan.end(), colorTable.begin(), [](auto&& color) {
                return static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ color });
            });
        }
        return colorTable;
    }

    std::span<winrt::Microsoft::Terminal::Core::Color> TerminalSettings::_getColorTableImpl()
    {
        if (_ColorTable.has_value())
        {
            return std::span{ *_ColorTable };
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
