#pragma once

#include "Model.Profile.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    struct Profile : ProfileT<Profile>
    {
    public:
        Profile() = default;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(guid, Guid, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, Name, _PropertyChangedHandlers, L"Default");
        OBSERVABLE_GETSET_PROPERTY(hstring, Source, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(guid, ConnectionType, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, Icon, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, Hidden, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(CloseOnExitMode, CloseOnExit, _PropertyChangedHandlers, CloseOnExitMode::graceful);
        OBSERVABLE_GETSET_PROPERTY(hstring, TabTitle, _PropertyChangedHandlers);

        // Terminal Control Settings
        OBSERVABLE_GETSET_PROPERTY(bool, UseAcrylic, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(double, AcrylicOpacity, _PropertyChangedHandlers, 0.5);
        OBSERVABLE_GETSET_PROPERTY(ScrollbarState, ScrollState, _PropertyChangedHandlers, ScrollbarState::visible);
        OBSERVABLE_GETSET_PROPERTY(hstring, FontFace, _PropertyChangedHandlers, L"Cascadia Mono");
        OBSERVABLE_GETSET_PROPERTY(int32_t, FontSize, _PropertyChangedHandlers, 12);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Text::FontWeight, FontWeight, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, Padding, _PropertyChangedHandlers, L"8, 8, 8, 8");
        OBSERVABLE_GETSET_PROPERTY(bool, CopyOnSelect, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(hstring, Commandline, _PropertyChangedHandlers, L"cmd.exe");
        OBSERVABLE_GETSET_PROPERTY(hstring, StartingDirectory, _PropertyChangedHandlers, L"%USERPROFILE%");
        OBSERVABLE_GETSET_PROPERTY(hstring, EnvironmentVariables, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, BackgroundImage, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(double, BackgroundImageOpacity, _PropertyChangedHandlers, 0.5);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, _PropertyChangedHandlers, Windows::UI::Xaml::Media::Stretch::UniformToFill);

    public:
        // BackgroundImageAlignment is 1 setting saved as 2 separate values
        Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept;
        void BackgroundImageHorizontalAlignment(const Windows::UI::Xaml::HorizontalAlignment& value) noexcept;
        Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept;
        void BackgroundImageVerticalAlignment(const Windows::UI::Xaml::VerticalAlignment& value) noexcept;

        OBSERVABLE_GETSET_PROPERTY(uint32_t, SelectionBackground, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(TextAntialiasingMode, AntialiasingMode, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, RetroTerminalEffect, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceFullRepaintRendering, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, SoftwareRendering, _PropertyChangedHandlers, false);

        // Terminal Core Settings
        OBSERVABLE_GETSET_PROPERTY(uint32_t, DefaultForeground, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, DefaultBackground, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, ColorScheme, _PropertyChangedHandlers, L"Campbell");
        OBSERVABLE_GETSET_PROPERTY(int32_t, HistorySize, _PropertyChangedHandlers, 9001);
        OBSERVABLE_GETSET_PROPERTY(int32_t, InitialRows, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(int32_t, InitialCols, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, SnapOnInput, _PropertyChangedHandlers, true);
        OBSERVABLE_GETSET_PROPERTY(bool, AltGrAliasing, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, CursorColor, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(CursorStyle, CursorShape, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, CursorHeight, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(hstring, StartingTitle, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, SuppressApplicationTitle, _PropertyChangedHandlers, false);
        OBSERVABLE_GETSET_PROPERTY(bool, ForceVTInput, _PropertyChangedHandlers, false);

    private:
        std::tuple<Windows::UI::Xaml::HorizontalAlignment, Windows::UI::Xaml::VerticalAlignment> _BackgroundImageAlignment;
    };
}
