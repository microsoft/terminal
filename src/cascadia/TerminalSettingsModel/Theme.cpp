// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Theme.h"
#include "../../types/inc/Utils.hpp"
#include "../../types/inc/colorTable.hpp"
#include "Utils.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "ThemeColor.g.cpp"
#include "WindowTheme.g.cpp"
#include "TabRowTheme.g.cpp"
#include "TabTheme.g.cpp"
#include "Theme.g.cpp"

using namespace ::Microsoft::Console;
using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::UI;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

static constexpr std::string_view NameKey{ "name" };

static constexpr wchar_t RegKeyDwm[] = L"Software\\Microsoft\\Windows\\DWM";
static constexpr wchar_t RegKeyAccentColor[] = L"AccentColor";

#define THEME_SETTINGS_COPY(type, name, jsonKey, ...) \
    result->_##name = _##name;

#define THEME_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);

#define THEME_OBJECT(className, macro)                    \
    winrt::com_ptr<className> className::Copy()           \
    {                                                     \
        auto result{ winrt::make_self<className>() };     \
        macro(THEME_SETTINGS_COPY);                       \
        return result;                                    \
    }                                                     \
                                                          \
    Json::Value className::ToJson()                       \
    {                                                     \
        Json::Value json{ Json::ValueType::objectValue }; \
        macro(THEME_SETTINGS_TO_JSON);                    \
        return json;                                      \
    }

THEME_OBJECT(WindowTheme, MTSM_THEME_WINDOW_SETTINGS);
THEME_OBJECT(TabRowTheme, MTSM_THEME_TABROW_SETTINGS);
THEME_OBJECT(TabTheme, MTSM_THEME_TAB_SETTINGS);

#undef THEME_SETTINGS_COPY
#undef THEME_SETTINGS_TO_JSON
#undef THEME_OBJECT

winrt::Microsoft::Terminal::Settings::Model::ThemeColor ThemeColor::FromColor(const winrt::Microsoft::Terminal::Core::Color& coreColor) noexcept
{
    auto result = winrt::make_self<implementation::ThemeColor>();
    result->_Color = coreColor;
    result->_ColorType = ThemeColorType::Color;
    return *result;
}

winrt::Microsoft::Terminal::Settings::Model::ThemeColor ThemeColor::FromAccent() noexcept
{
    auto result = winrt::make_self<implementation::ThemeColor>();
    result->_ColorType = ThemeColorType::Accent;
    return *result;
}

winrt::Microsoft::Terminal::Settings::Model::ThemeColor ThemeColor::FromTerminalBackground() noexcept
{
    auto result = winrt::make_self<implementation::ThemeColor>();
    result->_ColorType = ThemeColorType::TerminalBackground;
    return *result;
}

static wil::unique_hkey openDwmRegKey()
{
    HKEY hKey{ nullptr };
    if (RegOpenKeyEx(HKEY_CURRENT_USER, RegKeyDwm, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        return wil::unique_hkey{ hKey };
    }
    return nullptr;
}
static DWORD readDwmSubValue(const wil::unique_hkey& dwmRootKey, const wchar_t* key)
{
    DWORD val{ 0 };
    DWORD size{ sizeof(val) };
    LOG_IF_FAILED(RegQueryValueExW(dwmRootKey.get(), key, nullptr, nullptr, reinterpret_cast<BYTE*>(&val), &size));
    return val;
}

static til::color _getAccentColorForTitlebar()
{
    // The color used for the "Use Accent color in the title bar" in DWM is
    // stored in HKCU\Software\Microsoft\Windows\DWM\AccentColor.
    return til::color{ static_cast<COLORREF>(readDwmSubValue(openDwmRegKey(), RegKeyAccentColor)) }.with_alpha(255);
}

til::color ThemeColor::ColorFromBrush(const winrt::WUX::Media::Brush& brush)
{
    if (auto acrylic = brush.try_as<winrt::WUX::Media::AcrylicBrush>())
    {
        return acrylic.TintColor();
    }
    else if (auto solidColor = brush.try_as<winrt::WUX::Media::SolidColorBrush>())
    {
        return solidColor.Color();
    }
    return {};
}

winrt::WUX::Media::Brush ThemeColor::Evaluate(const winrt::WUX::ResourceDictionary& res,
                                              const winrt::WUX::Media::Brush& terminalBackground,
                                              const bool forTitlebar)
{
    static const auto accentColorKey{ winrt::box_value(L"SystemAccentColor") };

    // NOTE: Currently, the DWM titlebar is always drawn, underneath our XAML
    // content. If the opacity is <1.0, then you'll be able to see it, including
    // the original caption buttons, which we don't want.

    switch (ColorType())
    {
    case ThemeColorType::Accent:
    {
        // NOTE: There is no canonical way to get the unfocused ACCENT titlebar
        // color in Windows. Edge uses it's own heuristic, and in Windows 11,
        // much of this logic is rapidly changing. We're not gonna mess with
        // that, since it seems there's no good way to reverse engineer that.
        til::color accentColor = forTitlebar ?
                                     _getAccentColorForTitlebar() :
                                     til::color{ winrt::unbox_value<winrt::Windows::UI::Color>(res.Lookup(accentColorKey)) };

        const winrt::WUX::Media::SolidColorBrush accentBrush{ accentColor };
        // _getAccentColorForTitlebar should have already filled the alpha
        // channel in with 255
        return accentBrush;
    }
    case ThemeColorType::Color:
    {
        return winrt::WUX::Media::SolidColorBrush{ forTitlebar ?
                                                       Color().with_alpha(255) :
                                                       Color() };
    }
    case ThemeColorType::TerminalBackground:
    {
        // If we're evaluating this color for the tab row, there are some rules
        // we have to follow, unfortunately. We can't allow a transparent
        // background, so we have to make sure to fill that in with Opacity(1.0)
        // manually.
        //
        // So for that case, just make a new brush with the relevant properties
        // set.
        if (forTitlebar)
        {
            if (auto acrylic = terminalBackground.try_as<winrt::WUX::Media::AcrylicBrush>())
            {
                winrt::WUX::Media::AcrylicBrush newBrush{};
                newBrush.TintColor(acrylic.TintColor());
                newBrush.FallbackColor(acrylic.FallbackColor());
                newBrush.TintLuminosityOpacity(acrylic.TintLuminosityOpacity());

                // Allow acrylic opacity, but it's gotta be HostBackdrop acrylic.
                //
                // For now, just always use 50% opacity for this. If we do ever
                // figure out how to get rid of our titlebar under the XAML tab
                // row (GH#10509), we can always get rid of the HostBackdrop
                // thing, and all this copying, and just return the
                // terminalBackground brush directly.
                //
                // Because we're wholesale copying the brush, we won't be able
                // to adjust it's opacity with the mouse wheel. This seems like
                // an acceptable tradeoff for now.
                newBrush.TintOpacity(.5);
                newBrush.BackgroundSource(winrt::WUX::Media::AcrylicBackgroundSource::HostBackdrop);
                return newBrush;
            }
            else if (auto solidColor = terminalBackground.try_as<winrt::WUX::Media::SolidColorBrush>())
            {
                winrt::WUX::Media::SolidColorBrush newBrush{};
                newBrush.Color(til::color{ solidColor.Color() }.with_alpha(255));
                return newBrush;
            }
        }

        return terminalBackground;
    }
    }
    return nullptr;
}

#define THEME_SETTINGS_FROM_JSON(type, name, jsonKey, ...)                    \
    {                                                                         \
        std::optional<type> _val;                                             \
        _val = JsonUtils::GetValueForKey<std::optional<type>>(json, jsonKey); \
        if (_val)                                                             \
            result->name(*_val);                                              \
    }

#define THEME_OBJECT_CONVERTER(nameSpace, name, macro)                                         \
    template<>                                                                                 \
    struct ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<nameSpace::name> \
    {                                                                                          \
        nameSpace::name FromJson(const Json::Value& json)                                      \
        {                                                                                      \
            if (json == Json::Value::null)                                                     \
                return nullptr;                                                                \
            auto result = winrt::make_self<nameSpace::implementation::name>();                 \
            macro(THEME_SETTINGS_FROM_JSON);                                                   \
            return *result;                                                                    \
        }                                                                                      \
                                                                                               \
        bool CanConvert(const Json::Value& json)                                               \
        {                                                                                      \
            return json.isObject();                                                            \
        }                                                                                      \
                                                                                               \
        Json::Value ToJson(const nameSpace::name& val)                                         \
        {                                                                                      \
            return val ? winrt::get_self<name>(val)->ToJson() : Json::Value::null;             \
        }                                                                                      \
                                                                                               \
        std::string TypeDescription() const                                                    \
        {                                                                                      \
            return "name (You should never see this)";                                         \
        }                                                                                      \
    };

THEME_OBJECT_CONVERTER(winrt::Microsoft::Terminal::Settings::Model, WindowTheme, MTSM_THEME_WINDOW_SETTINGS);
THEME_OBJECT_CONVERTER(winrt::Microsoft::Terminal::Settings::Model, TabRowTheme, MTSM_THEME_TABROW_SETTINGS);
THEME_OBJECT_CONVERTER(winrt::Microsoft::Terminal::Settings::Model, TabTheme, MTSM_THEME_TAB_SETTINGS);

#undef THEME_SETTINGS_FROM_JSON
#undef THEME_SETTINGS_TO_JSON
#undef THEME_OBJECT_CONVERTER

Theme::Theme(const winrt::WUX::ElementTheme& requestedTheme) noexcept
{
    auto window{ winrt::make_self<implementation::WindowTheme>() };
    window->RequestedTheme(requestedTheme);
    _Window = *window;
}

winrt::com_ptr<Theme> Theme::Copy() const
{
    auto theme{ winrt::make_self<Theme>() };

    theme->_Name = _Name;

    if (_Window)
    {
        theme->_Window = *winrt::get_self<implementation::WindowTheme>(_Window)->Copy();
    }
    if (_TabRow)
    {
        theme->_TabRow = *winrt::get_self<implementation::TabRowTheme>(_TabRow)->Copy();
    }
    if (_Tab)
    {
        theme->_Tab = *winrt::get_self<implementation::TabTheme>(_Tab)->Copy();
    }

    return theme;
}

// Method Description:
// - Create a new instance of this class from a serialized JsonObject.
// Arguments:
// - json: an object which should be a serialization of a ColorScheme object.
// Return Value:
// - Returns nullptr for invalid JSON.
winrt::com_ptr<Theme> Theme::FromJson(const Json::Value& json)
{
    auto result = winrt::make_self<Theme>();

    JsonUtils::GetValueForKey(json, NameKey, result->_Name);

    // This will use each of the ConversionTrait's from above to quickly parse the sub-objects

#define THEME_SETTINGS_LAYER_JSON(type, name, jsonKey, ...)                   \
    {                                                                         \
        std::optional<type> _val;                                             \
        _val = JsonUtils::GetValueForKey<std::optional<type>>(json, jsonKey); \
        if (_val)                                                             \
            result->_##name = *_val;                                          \
        else                                                                  \
            result->_##name = nullptr;                                        \
    }

    MTSM_THEME_SETTINGS(THEME_SETTINGS_LAYER_JSON)
#undef THEME_SETTINGS_LAYER_JSON

    return result;
}

// Method Description:
// - Create a new serialized JsonObject from an instance of this class
// Arguments:
// - <none>
// Return Value:
// - the JsonObject representing this instance
Json::Value Theme::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, NameKey, _Name);

    // Don't serialize anything if the object is null.
#define THEME_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    if (_##name)                                         \
        JsonUtils::SetValueForKey(json, jsonKey, _##name);

    MTSM_THEME_SETTINGS(THEME_SETTINGS_TO_JSON)
#undef THEME_SETTINGS_TO_JSON

    return json;
}

winrt::hstring Theme::ToString()
{
    return Name();
}
// Method Description:
// - A helper for retrieving the RequestedTheme out of the window property.
//   There's a bunch of places throughout the app that all ask for the
//   RequestedTheme, this saves some hassle. If there wasn't a `window` defined
//   for this theme, this'll quickly just return `system`, to use the OS theme.
// Return Value:
// - the set applicationTheme for this Theme, otherwise the system theme.
winrt::WUX::ElementTheme Theme::RequestedTheme() const noexcept
{
    return _Window ? _Window.RequestedTheme() : winrt::WUX::ElementTheme::Default;
}
