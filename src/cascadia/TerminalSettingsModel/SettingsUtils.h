/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsUtils.h

Abstract:
- 

Author(s):
- 

--*/
#pragma once

#define GLOBAL_SETTINGS(X)                              \
    X(int32_t, InitialRows, 30)                         \
    X(int32_t, InitialCols, 80)                         \
    X(hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS) \
    X(bool, CopyOnSelect, false)                        \
    X(bool, FocusFollowMouse, false)                    \
    X(bool, ForceFullRepaintRendering, false)           \
    X(bool, SoftwareRendering, false)                   \
    X(bool, ForceVTInput, false)                        \
    X(bool, TrimBlockSelection, false)                  \
    X(bool, DetectURLs, true)

#define PROFILE_SETTINGS(X)                                                                                             \
    X(int32_t, HistorySize, DEFAULT_HISTORY_SIZE)                                                                       \
    X(bool, SnapOnInput, true)                                                                                          \
    X(bool, AltGrAliasing, true)                                                                                        \
    X(bool, UseAcrylic, false)                                                                                          \
    X(hstring, Padding, DEFAULT_PADDING)                                                                                \
    X(hstring, Commandline, L"cmd.exe")                                                                                 \
    X(Microsoft::Terminal::Control::ScrollbarState, ScrollState, Microsoft::Terminal::Control::ScrollbarState::Visible) \
    X(Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale)

#define FONT_SETTINGS(X)                                \
    X(hstring, FontFace, DEFAULT_FONT_FACE)             \
    X(int32_t, FontSize, DEFAULT_FONT_SIZE)             \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight) \
    X(IFontAxesMap, FontAxes)                           \
    X(IFontFeatureMap, FontFeatures)

#define APPEARANCE_SETTINGS(X)                                                                                                       \
    X(Core::CursorStyle, CursorShape, Core::CursorStyle::Bar)                                                                        \
    X(uint32_t, CursorHeight, DEFAULT_CURSOR_HEIGHT)                                                                                 \
    X(double, BackgroundImageOpacity, 1.0)                                                                                           \
    X(winrt::Windows::UI::Xaml::Media::Stretch, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch::UniformToFill) \
    X(bool, RetroTerminalEffect, false)                                                                                              \
    X(hstring, PixelShaderPath)
