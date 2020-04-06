// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalSettings.h"
#include <DefaultSettings.h>

#include "TerminalSettings.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    // Disable "default constructor may not throw."
    // We put default values into the hstrings here, which allocates and could throw.
    // Working around that situation is more headache than it's worth at the moment.
#pragma warning(suppress : 26455)
    TerminalSettings::TerminalSettings() :
        _defaultForeground{ DEFAULT_FOREGROUND_WITH_ALPHA },
        _defaultBackground{ DEFAULT_BACKGROUND_WITH_ALPHA },
        _selectionBackground{ DEFAULT_FOREGROUND },
        _colorTable{},
        _historySize{ DEFAULT_HISTORY_SIZE },
        _initialRows{ 30 },
        _initialCols{ 80 },
        _rowsToScroll{ 0 },
        _snapOnInput{ true },
        _cursorColor{ DEFAULT_CURSOR_COLOR },
        _cursorShape{ CursorStyle::Vintage },
        _cursorHeight{ DEFAULT_CURSOR_HEIGHT },
        _wordDelimiters{ DEFAULT_WORD_DELIMITERS },
        _copyOnSelect{ false },
        _copyFormatting{ false },
        _profileName{},
        _useAcrylic{ false },
        _tintOpacity{ 0.5 },
        _padding{ DEFAULT_PADDING },
        _fontFace{ DEFAULT_FONT_FACE },
        _fontSize{ DEFAULT_FONT_SIZE },
        _backgroundImage{},
        _backgroundImageOpacity{ 1.0 },
        _backgroundImageStretchMode{ winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill },
        _backgroundImageHorizontalAlignment{ winrt::Windows::UI::Xaml::HorizontalAlignment::Center },
        _backgroundImageVerticalAlignment{ winrt::Windows::UI::Xaml::VerticalAlignment::Center },
        _keyBindings{ nullptr },
        _scrollbarState{ ScrollbarState::Visible },
        _antialiasingMode{ TextAntialiasingMode::Grayscale }

    {
    }

    uint32_t TerminalSettings::DefaultForeground() noexcept
    {
        return _defaultForeground;
    }

    void TerminalSettings::DefaultForeground(uint32_t value) noexcept
    {
        _defaultForeground = value;
    }

    uint32_t TerminalSettings::DefaultBackground() noexcept
    {
        return _defaultBackground;
    }

    void TerminalSettings::DefaultBackground(uint32_t value) noexcept
    {
        _defaultBackground = value;
    }

    uint32_t TerminalSettings::SelectionBackground() noexcept
    {
        return _selectionBackground;
    }

    void TerminalSettings::SelectionBackground(uint32_t value) noexcept
    {
        _selectionBackground = value;
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

    int32_t TerminalSettings::HistorySize() noexcept
    {
        return _historySize;
    }

    void TerminalSettings::HistorySize(int32_t value) noexcept
    {
        _historySize = value;
    }

    int32_t TerminalSettings::InitialRows() noexcept
    {
        return _initialRows;
    }

    void TerminalSettings::InitialRows(int32_t value) noexcept
    {
        _initialRows = value;
    }

    int32_t TerminalSettings::InitialCols() noexcept
    {
        return _initialCols;
    }

    void TerminalSettings::InitialCols(int32_t value) noexcept
    {
        _initialCols = value;
    }

    int32_t TerminalSettings::RowsToScroll() noexcept
    {
        if (_rowsToScroll != 0)
        {
            return _rowsToScroll;
        }
        int systemRowsToScroll;
        if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &systemRowsToScroll, 0))
        {
            systemRowsToScroll = 4;
        }
        return systemRowsToScroll;
    }

    void TerminalSettings::RowsToScroll(int32_t value) noexcept
    {
        _rowsToScroll = value;
    }

    bool TerminalSettings::SnapOnInput() noexcept
    {
        return _snapOnInput;
    }

    void TerminalSettings::SnapOnInput(bool value) noexcept
    {
        _snapOnInput = value;
    }

    uint32_t TerminalSettings::CursorColor() noexcept
    {
        return _cursorColor;
    }

    void TerminalSettings::CursorColor(uint32_t value) noexcept
    {
        _cursorColor = value;
    }

    Settings::CursorStyle TerminalSettings::CursorShape() const noexcept
    {
        return _cursorShape;
    }

    void TerminalSettings::CursorShape(Settings::CursorStyle const& value) noexcept
    {
        _cursorShape = value;
    }

    uint32_t TerminalSettings::CursorHeight() noexcept
    {
        return _cursorHeight;
    }

    void TerminalSettings::CursorHeight(uint32_t value) noexcept
    {
        _cursorHeight = value;
    }

    hstring TerminalSettings::WordDelimiters()
    {
        return _wordDelimiters;
    }

    void TerminalSettings::WordDelimiters(hstring const& value)
    {
        _wordDelimiters = value;
    }

    bool TerminalSettings::CopyOnSelect() noexcept
    {
        return _copyOnSelect;
    }

    void TerminalSettings::CopyOnSelect(bool value) noexcept
    {
        _copyOnSelect = value;
    }

    bool TerminalSettings::CopyFormatting() noexcept
    {
        return _copyFormatting;
    }

    void TerminalSettings::CopyFormatting(bool value) noexcept
    {
        _copyFormatting = value;
    }

    void TerminalSettings::ProfileName(hstring const& value)
    {
        _profileName = value;
    }

    hstring TerminalSettings::ProfileName()
    {
        return _profileName;
    }

    bool TerminalSettings::UseAcrylic() noexcept
    {
        return _useAcrylic;
    }

    void TerminalSettings::UseAcrylic(bool value) noexcept
    {
        _useAcrylic = value;
    }

    double TerminalSettings::TintOpacity() noexcept
    {
        return _tintOpacity;
    }

    void TerminalSettings::TintOpacity(double value) noexcept
    {
        _tintOpacity = value;
    }

    hstring TerminalSettings::Padding()
    {
        return _padding;
    }

    void TerminalSettings::Padding(hstring value)
    {
        _padding = value;
    }

    hstring TerminalSettings::FontFace()
    {
        return _fontFace;
    }

    void TerminalSettings::FontFace(hstring const& value)
    {
        _fontFace = value;
    }

    int32_t TerminalSettings::FontSize() noexcept
    {
        return _fontSize;
    }

    void TerminalSettings::FontSize(int32_t value) noexcept
    {
        _fontSize = value;
    }

    void TerminalSettings::BackgroundImage(hstring const& value)
    {
        _backgroundImage = value;
    }

    hstring TerminalSettings::BackgroundImage()
    {
        return _backgroundImage;
    }

    void TerminalSettings::BackgroundImageOpacity(double value) noexcept
    {
        _backgroundImageOpacity = value;
    }

    double TerminalSettings::BackgroundImageOpacity() noexcept
    {
        return _backgroundImageOpacity;
    }

    winrt::Windows::UI::Xaml::Media::Stretch TerminalSettings::BackgroundImageStretchMode() noexcept
    {
        return _backgroundImageStretchMode;
    }

    void TerminalSettings::BackgroundImageStretchMode(winrt::Windows::UI::Xaml::Media::Stretch value) noexcept
    {
        _backgroundImageStretchMode = value;
    }

    winrt::Windows::UI::Xaml::HorizontalAlignment TerminalSettings::BackgroundImageHorizontalAlignment() noexcept
    {
        return _backgroundImageHorizontalAlignment;
    }

    void TerminalSettings::BackgroundImageHorizontalAlignment(winrt::Windows::UI::Xaml::HorizontalAlignment value) noexcept
    {
        _backgroundImageHorizontalAlignment = value;
    }

    winrt::Windows::UI::Xaml::VerticalAlignment TerminalSettings::BackgroundImageVerticalAlignment() noexcept
    {
        return _backgroundImageVerticalAlignment;
    }

    void TerminalSettings::BackgroundImageVerticalAlignment(winrt::Windows::UI::Xaml::VerticalAlignment value) noexcept
    {
        _backgroundImageVerticalAlignment = value;
    }

    Settings::IKeyBindings TerminalSettings::KeyBindings() noexcept
    {
        return _keyBindings;
    }

    void TerminalSettings::KeyBindings(Settings::IKeyBindings const& value) noexcept
    {
        _keyBindings = value;
    }

    hstring TerminalSettings::Commandline()
    {
        return _commandline;
    }

    void TerminalSettings::Commandline(hstring const& value)
    {
        _commandline = value;
    }

    hstring TerminalSettings::StartingDirectory()
    {
        return _startingDir;
    }

    void TerminalSettings::StartingDirectory(hstring const& value)
    {
        _startingDir = value;
    }

    hstring TerminalSettings::StartingTitle()
    {
        return _startingTitle;
    }

    void TerminalSettings::StartingTitle(hstring const& value)
    {
        _startingTitle = value;
    }

    bool TerminalSettings::SuppressApplicationTitle() noexcept
    {
        return _suppressApplicationTitle;
    }

    void TerminalSettings::SuppressApplicationTitle(bool value) noexcept
    {
        _suppressApplicationTitle = value;
    }

    hstring TerminalSettings::EnvironmentVariables()
    {
        return _envVars;
    }

    void TerminalSettings::EnvironmentVariables(hstring const& value)
    {
        _envVars = value;
    }

    Settings::ScrollbarState TerminalSettings::ScrollState() const noexcept
    {
        return _scrollbarState;
    }

    void TerminalSettings::ScrollState(Settings::ScrollbarState const& value) noexcept
    {
        _scrollbarState = value;
    }

    bool TerminalSettings::RetroTerminalEffect() noexcept
    {
        return _retroTerminalEffect;
    }

    void TerminalSettings::RetroTerminalEffect(bool value) noexcept
    {
        _retroTerminalEffect = value;
    }

    Settings::TextAntialiasingMode TerminalSettings::AntialiasingMode() const noexcept
    {
        return _antialiasingMode;
    }

    void TerminalSettings::AntialiasingMode(const Settings::TextAntialiasingMode& value) noexcept
    {
        _antialiasingMode = value;
    }

}
