// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// --------------------------- Core Appearance ---------------------------
//  All of these settings are defined in ICoreAppearance.
#define CORE_APPEARANCE_SETTINGS(X)                                                                                       \
    X(til::color, DefaultForeground, DEFAULT_FOREGROUND)                                                                  \
    X(til::color, DefaultBackground, DEFAULT_BACKGROUND)                                                                  \
    X(til::color, CursorColor, DEFAULT_CURSOR_COLOR)                                                                      \
    X(winrt::Microsoft::Terminal::Core::CursorStyle, CursorShape, winrt::Microsoft::Terminal::Core::CursorStyle::Vintage) \
    X(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT)                                                                      \
    X(bool, IntenseIsBold)                                                                                                \
    X(bool, IntenseIsBright, true)                                                                                        \
    X(winrt::Microsoft::Terminal::Core::AdjustTextMode, AdjustIndistinguishableColors, winrt::Microsoft::Terminal::Core::AdjustTextMode::Never)

// --------------------------- Control Appearance ---------------------------
//  All of these settings are defined in IControlAppearance.
#define CONTROL_APPEARANCE_SETTINGS(X)                                                                                                          \
    X(til::color, SelectionBackground, DEFAULT_FOREGROUND)                                                                                      \
    X(double, Opacity, 1.0)                                                                                                                     \
    X(winrt::hstring, BackgroundImage)                                                                                                          \
    X(double, BackgroundImageOpacity, 1.0)                                                                                                      \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill)            \
    X(winrt::Windows::UI::Xaml::HorizontalAlignment, BackgroundImageHorizontalAlignment, winrt::Windows::UI::Xaml::HorizontalAlignment::Center) \
    X(winrt::Windows::UI::Xaml::VerticalAlignment, BackgroundImageVerticalAlignment, winrt::Windows::UI::Xaml::VerticalAlignment::Center)       \
    X(bool, RetroTerminalEffect, false)                                                                                                         \
    X(winrt::hstring, PixelShaderPath)

// --------------------------- Core Settings ---------------------------
//  All of these settings are defined in ICoreSettings.
#define CORE_SETTINGS(X)                                                                                          \
    X(int32_t, HistorySize, DEFAULT_HISTORY_SIZE)                                                                 \
    X(int32_t, InitialRows, 30)                                                                                   \
    X(int32_t, InitialCols, 80)                                                                                   \
    X(bool, SnapOnInput, true)                                                                                    \
    X(bool, AltGrAliasing, true)                                                                                  \
    X(winrt::hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS)                                                    \
    X(bool, CopyOnSelect, false)                                                                                  \
    X(bool, FocusFollowMouse, false)                                                                              \
    X(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, TabColor, nullptr)         \
    X(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr) \
    X(bool, TrimBlockSelection, true)                                                                             \
    X(bool, SuppressApplicationTitle)                                                                             \
    X(bool, ForceVTInput, false)                                                                                  \
    X(winrt::hstring, StartingTitle)                                                                              \
    X(bool, DetectURLs, true)                                                                                     \
    X(bool, VtPassthrough, false)                                                                                 \
    X(bool, AutoMarkPrompts)                                                                                      \
    X(bool, RepositionCursorWithMouse, false)

// --------------------------- Control Settings ---------------------------
//  All of these settings are defined in IControlSettings.
#define CONTROL_SETTINGS(X)                                                                                                                              \
    X(winrt::hstring, ProfileName)                                                                                                                       \
    X(winrt::hstring, ProfileSource)                                                                                                                     \
    X(bool, UseAcrylic, false)                                                                                                                           \
    X(winrt::hstring, Padding, DEFAULT_PADDING)                                                                                                          \
    X(winrt::hstring, FontFace, L"Consolas")                                                                                                             \
    X(float, FontSize, DEFAULT_FONT_SIZE)                                                                                                                \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight)                                                                                                  \
    X(IFontFeatureMap, FontFeatures)                                                                                                                     \
    X(IFontAxesMap, FontAxes)                                                                                                                            \
    X(winrt::hstring, CellWidth)                                                                                                                         \
    X(winrt::hstring, CellHeight)                                                                                                                        \
    X(winrt::Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr)                                                                           \
    X(winrt::hstring, Commandline)                                                                                                                       \
    X(winrt::hstring, StartingDirectory)                                                                                                                 \
    X(winrt::Microsoft::Terminal::Control::ScrollbarState, ScrollState, winrt::Microsoft::Terminal::Control::ScrollbarState::Visible)                    \
    X(winrt::Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale) \
    X(bool, ForceFullRepaintRendering, false)                                                                                                            \
    X(bool, SoftwareRendering, false)                                                                                                                    \
    X(bool, UseAtlasEngine, false)                                                                                                                       \
    X(bool, UseBackgroundImageForWindow, false)                                                                                                          \
    X(bool, ShowMarks, false)                                                                                                                            \
    X(bool, RightClickContextMenu, false)
