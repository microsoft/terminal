// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Text.h>

namespace Microsoft::Terminal::Settings::Model::Native
{
    enum class OriginTag : int32_t
    {
        None,
        User,
        InBox,
        Generated,
        Fragment,
        ProfilesDefaults
    };

    enum class ScrollbarState : int32_t
    {
        Visible,
        Hidden,
        Always
    };

    enum class TextAntialiasingMode : int32_t
    {
        Grayscale,
        Cleartype,
        Aliased
    };

    enum class PathTranslationStyle : int32_t
    {
        None,
        WSL,
        Cygwin,
        MSYS2,
        MinGW
    };

    enum class CloseOnExitMode : int32_t
    {
        Never,
        Graceful,
        Always,
        Automatic
    };

    enum class BellStyle : uint32_t
    {
        Audible = 0x1,
        Window = 0x2,
        Taskbar = 0x4,
        Notification = 0x8,
        All = 0xffffffff
    };

    enum class CursorStyle : int32_t
    {
        Vintage,
        Bar,
        Underscore,
        DoubleUnderscore,
        FilledBox,
        EmptyBox
    };

    enum class AdjustTextMode : int32_t
    {
        Never,
        Indexed,
        Always,
        Automatic
    };

    enum class Stretch : int32_t
    {
        None,
        Fill,
        Uniform,
        UniformToFill
    };

    enum class ConvergedAlignment : uint32_t
    {
        Horizontal_Center = 0x00,
        Horizontal_Left = 0x01,
        Horizontal_Right = 0x02,
        Vertical_Center = 0x00,
        Vertical_Top = 0x10,
        Vertical_Bottom = 0x20
    };

    enum class IntenseStyle : uint32_t
    {
        Bold = 0x1,
        Bright = 0x2,
        All = 0xffffffff
    };

    struct Color
    {
        uint8_t R;
        uint8_t G;
        uint8_t B;
        uint8_t A;

        bool operator==(const Color&) const noexcept = default;
    };

    using EnvironmentVariableMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>;
    using FontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
    using FontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
}
