/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TerminalSettings.h

Abstract:
- The implementation of the TerminalSettings winrt class. Provides both
        terminal control settings and terminal core settings.
Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include <conattrs.hpp>
#include "TerminalSettings.g.h"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    struct TerminalSettings : TerminalSettingsT<TerminalSettings>
    {
        TerminalSettings();

        // --------------------------- Core Settings ---------------------------
        //  All of these settings are defined in ICoreSettings.
        uint32_t DefaultForeground() noexcept;
        void DefaultForeground(uint32_t value) noexcept;
        uint32_t DefaultBackground() noexcept;
        void DefaultBackground(uint32_t value) noexcept;
        uint32_t SelectionBackground() noexcept;
        void SelectionBackground(uint32_t value) noexcept;
        uint32_t GetColorTableEntry(int32_t index) const noexcept;
        void SetColorTableEntry(int32_t index, uint32_t value);
        int32_t HistorySize() noexcept;
        void HistorySize(int32_t value) noexcept;
        int32_t InitialRows() noexcept;
        void InitialRows(int32_t value) noexcept;
        int32_t InitialCols() noexcept;
        void InitialCols(int32_t value) noexcept;
        int32_t RowsToScroll() noexcept;
        void RowsToScroll(int32_t value) noexcept;
        bool SnapOnInput() noexcept;
        void SnapOnInput(bool value) noexcept;
        uint32_t CursorColor() noexcept;
        void CursorColor(uint32_t value) noexcept;
        CursorStyle CursorShape() const noexcept;
        void CursorShape(winrt::Microsoft::Terminal::Settings::CursorStyle const& value) noexcept;
        uint32_t CursorHeight() noexcept;
        void CursorHeight(uint32_t value) noexcept;
        hstring WordDelimiters();
        void WordDelimiters(hstring const& value);
        bool CopyOnSelect() noexcept;
        void CopyOnSelect(bool value) noexcept;
        bool CopyFormatting() noexcept;
        void CopyFormatting(bool value) noexcept;
        // ------------------------ End of Core Settings -----------------------

        hstring ProfileName();
        void ProfileName(hstring const& value);
        bool UseAcrylic() noexcept;
        void UseAcrylic(bool value) noexcept;
        double TintOpacity() noexcept;
        void TintOpacity(double value) noexcept;
        hstring Padding();
        void Padding(hstring value);

        hstring FontFace();
        void FontFace(hstring const& value);
        int32_t FontSize() noexcept;
        void FontSize(int32_t value) noexcept;

        hstring BackgroundImage();
        void BackgroundImage(hstring const& value);
        double BackgroundImageOpacity() noexcept;
        void BackgroundImageOpacity(double value) noexcept;
        winrt::Windows::UI::Xaml::Media::Stretch BackgroundImageStretchMode() noexcept;
        void BackgroundImageStretchMode(winrt::Windows::UI::Xaml::Media::Stretch value) noexcept;
        winrt::Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() noexcept;
        void BackgroundImageHorizontalAlignment(winrt::Windows::UI::Xaml::HorizontalAlignment value) noexcept;
        winrt::Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() noexcept;
        void BackgroundImageVerticalAlignment(winrt::Windows::UI::Xaml::VerticalAlignment value) noexcept;

        winrt::Microsoft::Terminal::Settings::IKeyBindings KeyBindings() noexcept;
        void KeyBindings(winrt::Microsoft::Terminal::Settings::IKeyBindings const& value) noexcept;

        hstring Commandline();
        void Commandline(hstring const& value);

        hstring StartingDirectory();
        void StartingDirectory(hstring const& value);

        hstring StartingTitle();
        void StartingTitle(hstring const& value);

        bool SuppressApplicationTitle() noexcept;
        void SuppressApplicationTitle(bool value) noexcept;

        hstring EnvironmentVariables();
        void EnvironmentVariables(hstring const& value);

        ScrollbarState ScrollState() const noexcept;
        void ScrollState(winrt::Microsoft::Terminal::Settings::ScrollbarState const& value) noexcept;

        bool RetroTerminalEffect() noexcept;
        void RetroTerminalEffect(bool value) noexcept;

        TextAntialiasingMode AntialiasingMode() const noexcept;
        void AntialiasingMode(winrt::Microsoft::Terminal::Settings::TextAntialiasingMode const& value) noexcept;

    private:
        uint32_t _defaultForeground;
        uint32_t _defaultBackground;
        uint32_t _selectionBackground;
        std::array<uint32_t, COLOR_TABLE_SIZE> _colorTable;
        int32_t _historySize;
        int32_t _initialRows;
        int32_t _initialCols;
        int32_t _rowsToScroll;
        bool _snapOnInput;
        uint32_t _cursorColor;
        Settings::CursorStyle _cursorShape;
        uint32_t _cursorHeight;
        hstring _wordDelimiters;

        hstring _profileName;
        bool _useAcrylic;
        double _tintOpacity;
        hstring _fontFace;
        int32_t _fontSize;
        hstring _padding;
        hstring _backgroundImage;
        double _backgroundImageOpacity;
        winrt::Windows::UI::Xaml::Media::Stretch _backgroundImageStretchMode;
        winrt::Windows::UI::Xaml::HorizontalAlignment _backgroundImageHorizontalAlignment;
        winrt::Windows::UI::Xaml::VerticalAlignment _backgroundImageVerticalAlignment;
        bool _copyOnSelect;
        bool _copyFormatting;
        hstring _commandline;
        hstring _startingDir;
        hstring _startingTitle;
        bool _suppressApplicationTitle;
        hstring _envVars;
        Settings::IKeyBindings _keyBindings;
        Settings::ScrollbarState _scrollbarState;

        bool _retroTerminalEffect;

        Settings::TextAntialiasingMode _antialiasingMode;
    };
}

namespace winrt::Microsoft::Terminal::Settings::factory_implementation
{
    struct TerminalSettings : TerminalSettingsT<TerminalSettings, implementation::TerminalSettings>
    {
    };
}
