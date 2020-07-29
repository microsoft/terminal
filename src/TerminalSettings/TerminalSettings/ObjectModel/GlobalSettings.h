#pragma once
#include "ObjectModel.GlobalSettings.g.h"
#include <winrt/Windows.UI.h>

// Use this macro to quick implement both the getter and setter for a property.
// This should only be used for simple types where there's no logic in the
// getter/setter beyond just accessing/updating the value.
#define GETSET_PROPERTY(type, name, ...)                                                              \
public:                                                                                               \
    type name() const noexcept { return _##name; }                                                    \
    void name(const type& value) noexcept                                                             \
    {                                                                                                 \
        if (value != _##name)                                                                         \
        {                                                                                             \
            _##name = value;                                                                          \
            m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"##name" }); \
        }                                                                                             \
    }                                                                                                 \
                                                                                                      \
private:                                                                                              \
    type _##name{ __VA_ARGS__ };

#define DEFINE_PROPERTYCHANGED()                                                                     \
public:                                                                                              \
    event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler) \
    {                                                                                                \
        return m_propertyChanged.add(handler);                                                       \
    }                                                                                                \
                                                                                                     \
    void PropertyChanged(event_token const& token)                                                   \
    {                                                                                                \
        m_propertyChanged.remove(token);                                                             \
    }                                                                                                \
                                                                                                     \
private:                                                                                             \
    winrt::event<Windows::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;

namespace winrt::ObjectModel::implementation
{
    struct GlobalSettings : GlobalSettingsT<GlobalSettings>
    {
    public:
        GlobalSettings() = default;

        GETSET_PROPERTY(hstring, DefaultProfile, L"{61c54bbd-c2c6-5271-96e7-009a87ff44bf}");
        GETSET_PROPERTY(int, InitialRows, 120);
        GETSET_PROPERTY(int, InitialCols, 30);
        GETSET_PROPERTY(bool, AlwaysShowTabs, true);
        GETSET_PROPERTY(bool, ShowTitlebar, true);
        GETSET_PROPERTY(bool, ShowTitleInTitlebar, true);
        GETSET_PROPERTY(bool, ConfirmCloseAllTabs, true);
        GETSET_PROPERTY(Windows::UI::Xaml::ElementTheme, Theme, Windows::UI::Xaml::ElementTheme::Default);
        GETSET_PROPERTY(TabViewWidthMode, TabWidthMode, TabViewWidthMode::equal);
        GETSET_PROPERTY(bool, ShowTabsInTitlebar, true);
        GETSET_PROPERTY(hstring, WordDelimiters, L" /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502");
        GETSET_PROPERTY(bool, CopyOnSelect, false);
        GETSET_PROPERTY(bool, CopyFormatting, true);
        GETSET_PROPERTY(bool, WarnAboutLargePaste, true);
        GETSET_PROPERTY(bool, WarnAboutMultiLinePaste, true);
        GETSET_PROPERTY(hstring, LaunchPosition, L"(0,0)");
        GETSET_PROPERTY(AppLaunchMode, LaunchMode, AppLaunchMode::Default);
        GETSET_PROPERTY(bool, SnapToGridOnResize, true);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
        GETSET_PROPERTY(bool, SoftwareRendering, false);
        GETSET_PROPERTY(bool, ForceVTInput, true);
        GETSET_PROPERTY(bool, DebugFeaturesEnabled, false);
        GETSET_PROPERTY(bool, StartOnUserLogin, false);
        GETSET_PROPERTY(bool, AlwaysOnTop, false);
        GETSET_PROPERTY(bool, DisableDynamicProfiles, false);
        DEFINE_PROPERTYCHANGED();
    };
}
