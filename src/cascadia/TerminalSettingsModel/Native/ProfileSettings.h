// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define NATIVE_PROFILE_SETTINGS(X)                                                                 \
    X(int32_t, HistorySize, "historySize", DEFAULT_HISTORY_SIZE)                                    \
    X(bool, SnapOnInput, "snapOnInput", true)                                                       \
    X(bool, AltGrAliasing, "altGrAliasing", true)                                                   \
    X(winrt::hstring, AnswerbackMessage, "answerbackMessage")                                       \
    X(winrt::hstring, Commandline, "commandline", L"%SystemRoot%\\System32\\cmd.exe")                \
    X(Native::ScrollbarState, ScrollState, "scrollbarState", Native::ScrollbarState::Visible)      \
    X(Native::TextAntialiasingMode, AntialiasingMode, "antialiasingMode", Native::TextAntialiasingMode::Grayscale) \
    X(winrt::hstring, StartingDirectory, "startingDirectory")                                      \
    X(winrt::com_ptr<Native::MediaResource>, Icon, "icon", Native::MediaResource::FromString(L"\uE756")) \
    X(bool, SuppressApplicationTitle, "suppressApplicationTitle", false)                            \
    X(winrt::guid, ConnectionType, "connectionType")                                               \
    X(Native::CloseOnExitMode, CloseOnExit, "closeOnExit", Native::CloseOnExitMode::Automatic)       \
    X(winrt::hstring, TabTitle, "tabTitle")                                                        \
    X(Native::BellStyle, BellStyle, "bellStyle", Native::BellStyle::Audible)                         \
    X(Native::EnvironmentVariableMap, EnvironmentVariables, "environment", nullptr)                \
    X(bool, RightClickContextMenu, "rightClickContextMenu", false)                                  \
    X(winrt::com_ptr<Native::MediaResourceList>, BellSound, "bellSound", nullptr)                    \
    X(bool, Elevate, "elevate", false)                                                             \
    X(bool, AutoMarkPrompts, "autoMarkPrompts", true)                                               \
    X(bool, ShowMarks, "showMarksOnScrollbar", false)                                               \
    X(bool, RepositionCursorWithMouse, "experimental.repositionCursorWithMouse", false)             \
    X(bool, ReloadEnvironmentVariables, "compatibility.reloadEnvironmentVariables", true)          \
    X(bool, RainbowSuggestions, "experimental.rainbowSuggestions", false)                           \
    X(bool, ForceVTInput, "compatibility.input.forceVT", false)                                     \
    X(bool, AllowKittyKeyboardMode, "compatibility.kittyKeyboardMode", true)                         \
    X(bool, AllowVtChecksumReport, "compatibility.allowDECRQCRA", false)                             \
    X(bool, AllowVtClipboardWrite, "compatibility.allowOSC52", true)                                 \
    X(bool, AllowOscNotifications, "compatibility.allowOSC777", false)                              \
    X(bool, AllowKeypadMode, "compatibility.allowDECNKM", false)                                    \
    X(winrt::hstring, DragDropDelimiter, "dragDropDelimiter", L" ")                                 \
    X(Native::PathTranslationStyle, PathTranslationStyle, "pathTranslationStyle", Native::PathTranslationStyle::None)

#define NATIVE_FONT_SETTINGS(X)                                                                   \
    X(winrt::hstring, FontFace, "face", DEFAULT_FONT_FACE)                                         \
    X(float, FontSize, "size", DEFAULT_FONT_SIZE)                                                  \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight, "weight", DEFAULT_FONT_WEIGHT)              \
    X(Native::FontAxesMap, FontAxes, "axes")                                                       \
    X(Native::FontFeatureMap, FontFeatures, "features")                                            \
    X(bool, EnableBuiltinGlyphs, "builtinGlyphs", true)                                            \
    X(bool, EnableColorGlyphs, "colorGlyphs", true)                                                \
    X(winrt::hstring, CellWidth, "cellWidth")                                                      \
    X(winrt::hstring, CellHeight, "cellHeight")

#define NATIVE_APPEARANCE_SETTINGS(X)                                                              \
    X(Native::CursorStyle, CursorShape, "cursorShape", Native::CursorStyle::Bar)                    \
    X(uint32_t, CursorHeight, "cursorHeight", DEFAULT_CURSOR_HEIGHT)                               \
    X(float, BackgroundImageOpacity, "backgroundImageOpacity", 1.0f)                               \
    X(Native::Stretch, BackgroundImageStretchMode, "backgroundImageStretchMode", Native::Stretch::UniformToFill) \
    X(bool, RetroTerminalEffect, "experimental.retroTerminalEffect", false)                         \
    X(winrt::com_ptr<Native::MediaResource>, PixelShaderPath, "experimental.pixelShaderPath", Native::MediaResource::Empty()) \
    X(winrt::com_ptr<Native::MediaResource>, PixelShaderImagePath, "experimental.pixelShaderImagePath", Native::MediaResource::Empty()) \
    X(Native::ConvergedAlignment, BackgroundImageAlignment, "backgroundImageAlignment", Native::ConvergedAlignment::Horizontal_Center) \
    X(winrt::com_ptr<Native::MediaResource>, BackgroundImagePath, "backgroundImage", Native::MediaResource::Empty()) \
    X(Native::IntenseStyle, IntenseTextStyle, "intenseTextStyle", Native::IntenseStyle::Bright)       \
    X(Native::AdjustTextMode, AdjustIndistinguishableColors, "adjustIndistinguishableColors", Native::AdjustTextMode::Automatic) \
    X(bool, UseAcrylic, "useAcrylic", false)
