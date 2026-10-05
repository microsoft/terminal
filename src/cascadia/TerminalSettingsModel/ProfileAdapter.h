// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/Profile.h"
#include "Native/FontConfig.h"
#include "Native/AppearanceConfig.h"
#include "MediaResourceAdapter.h"

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    template<typename T>
    T ToNative(T value)
    {
        return value;
    }

    template<typename T>
    T ToProjected(T value)
    {
        return value;
    }

#define NATIVE_ENUM_ADAPTER(nativeType, projectedType)                              \
    inline Native::nativeType ToNative(const projectedType value) noexcept          \
    { return static_cast<Native::nativeType>(value); }                              \
    inline projectedType ToProjected(const Native::nativeType value) noexcept       \
    { return static_cast<projectedType>(value); }

    NATIVE_ENUM_ADAPTER(OriginTag, winrt::Microsoft::Terminal::Settings::Model::OriginTag)
    NATIVE_ENUM_ADAPTER(ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState)
    NATIVE_ENUM_ADAPTER(TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode)
    NATIVE_ENUM_ADAPTER(PathTranslationStyle, winrt::Microsoft::Terminal::Control::PathTranslationStyle)
    NATIVE_ENUM_ADAPTER(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode)
    NATIVE_ENUM_ADAPTER(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle)
    NATIVE_ENUM_ADAPTER(CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle)
    NATIVE_ENUM_ADAPTER(AdjustTextMode, winrt::Microsoft::Terminal::Core::AdjustTextMode)
    NATIVE_ENUM_ADAPTER(Stretch, winrt::Windows::UI::Xaml::Media::Stretch)
    NATIVE_ENUM_ADAPTER(ConvergedAlignment, winrt::Microsoft::Terminal::Settings::Model::ConvergedAlignment)
    NATIVE_ENUM_ADAPTER(IntenseStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle)

#undef NATIVE_ENUM_ADAPTER

    inline Native::Color ToNative(const winrt::Microsoft::Terminal::Core::Color& value) noexcept
    {
        return { value.R, value.G, value.B, value.A };
    }

    inline winrt::Microsoft::Terminal::Core::Color ToProjected(const Native::Color& value) noexcept
    {
        return { value.R, value.G, value.B, value.A };
    }

    inline std::optional<Native::Color> ToNative(const winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>& value)
    {
        return value ? std::optional<Native::Color>{ ToNative(value.Value()) } : std::nullopt;
    }

    inline winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color> ToProjected(const std::optional<Native::Color>& value)
    {
        if (value)
        {
            return ToProjected(*value);
        }
        return nullptr;
    }

    TSM_NATIVE_API winrt::com_ptr<Native::Profile> ToNative(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);
    TSM_NATIVE_API winrt::Microsoft::Terminal::Settings::Model::Profile ToProjected(const winrt::com_ptr<Native::Profile>& profile);
    TSM_NATIVE_API winrt::com_ptr<Native::FontConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::FontConfig& font);
    TSM_NATIVE_API winrt::Microsoft::Terminal::Settings::Model::FontConfig ToProjected(const winrt::com_ptr<Native::FontConfig>& font);
    TSM_NATIVE_API winrt::com_ptr<Native::AppearanceConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig& appearance);
    TSM_NATIVE_API winrt::com_ptr<Native::AppearanceConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::AppearanceConfig& appearance);
    TSM_NATIVE_API winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig ToProjected(const winrt::com_ptr<Native::AppearanceConfig>& appearance);

    inline winrt::weak_ref<Native::Profile> ToNative(const winrt::weak_ref<winrt::Microsoft::Terminal::Settings::Model::Profile>& profile)
    {
        const auto native = ToNative(profile.get());
        return native ? native->get_weak() : winrt::weak_ref<Native::Profile>{};
    }
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    namespace Native = ::Microsoft::Terminal::Settings::Model::Native;
    namespace Adapters = ::Microsoft::Terminal::Settings::Model::Adapters;
}

#define TSM_ADAPTER_PROPERTY(nativeType, name)                                                   \
    auto name() const { return ::Microsoft::Terminal::Settings::Model::Adapters::ToProjected(_native->name()); } \
    void name(const decltype(::Microsoft::Terminal::Settings::Model::Adapters::ToProjected(std::declval<nativeType>()))& value) \
    { _native->name(::Microsoft::Terminal::Settings::Model::Adapters::ToNative(value)); }          \
    bool Has##name() const { return _native->Has##name(); }                                      \
    void Clear##name() { _native->Clear##name(); }                                               \
    auto name##OverrideSource() const                                                           \
    { return ::Microsoft::Terminal::Settings::Model::Adapters::ToProjected(_native->name##OverrideSource()); }
