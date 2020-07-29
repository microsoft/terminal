#pragma once

#include "ObjectModel.Profile.g.h"

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
    struct Profile : ProfileT<Profile>
    {
    public:
        Profile() = default;

        GETSET_PROPERTY(guid, Guid);
        GETSET_PROPERTY(hstring, Name, L"Default");
        GETSET_PROPERTY(hstring, Source);
        GETSET_PROPERTY(guid, ConnectionType);
        GETSET_PROPERTY(hstring, Icon);
        GETSET_PROPERTY(bool, Hidden, false);
        GETSET_PROPERTY(CloseOnExitMode, CloseOnExit);
        GETSET_PROPERTY(hstring, TabTitle);

        // Terminal Control Settings
        GETSET_PROPERTY(bool, UseAcrylic, false);
        GETSET_PROPERTY(double, AcrylicOpacity, 0.5);
        GETSET_PROPERTY(ScrollbarState, ScrollState);
        GETSET_PROPERTY(hstring, FontFace);
        GETSET_PROPERTY(int32_t, FontSize);
        GETSET_PROPERTY(Windows::UI::Text::FontWeight, FontWeight);
        GETSET_PROPERTY(hstring, Padding);
        GETSET_PROPERTY(bool, CopyOnSelect);
        GETSET_PROPERTY(hstring, Commandline, L"cmd.exe");
        GETSET_PROPERTY(hstring, StartingDirectory);
        GETSET_PROPERTY(hstring, EnvironmentVariables);
        GETSET_PROPERTY(hstring, BackgroundImage);
        GETSET_PROPERTY(double, BackgroundImageOpacity);
        GETSET_PROPERTY(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode);

    public:
        // BackgroundImageAlignment is 1 setting saved as 2 separate values
        Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept;
        void BackgroundImageHorizontalAlignment(const Windows::UI::Xaml::HorizontalAlignment& value) noexcept;
        Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept;
        void BackgroundImageVerticalAlignment(const Windows::UI::Xaml::VerticalAlignment& value) noexcept;

        GETSET_PROPERTY(uint32_t, SelectionBackground);
        GETSET_PROPERTY(TextAntialiasingMode, AntialiasingMode);
        GETSET_PROPERTY(bool, RetroTerminalEffect);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering);
        GETSET_PROPERTY(bool, SoftwareRendering);

        // Terminal Core Settings
        GETSET_PROPERTY(uint32_t, DefaultForeground);
        GETSET_PROPERTY(uint32_t, DefaultBackground);
        GETSET_PROPERTY(hstring, ColorScheme, L"Campbell");
        GETSET_PROPERTY(int32_t, HistorySize);
        GETSET_PROPERTY(int32_t, InitialRows);
        GETSET_PROPERTY(int32_t, InitialCols);
        GETSET_PROPERTY(bool, SnapOnInput);
        GETSET_PROPERTY(bool, AltGrAliasing);
        GETSET_PROPERTY(uint32_t, CursorColor);
        GETSET_PROPERTY(CursorStyle, CursorShape);
        GETSET_PROPERTY(uint32_t, CursorHeight);
        GETSET_PROPERTY(hstring, StartingTitle);
        GETSET_PROPERTY(bool, SuppressApplicationTitle);
        GETSET_PROPERTY(bool, ForceVTInput);

        DEFINE_PROPERTYCHANGED();

    private:
        std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment> _BackgroundImageAlignment;
    };
}
