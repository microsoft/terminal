// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FontConfig.h"
#include "FontConfig.g.cpp"
#include "AdapterCache.h"

namespace
{
    using FontAdapter = winrt::Microsoft::Terminal::Settings::Model::implementation::FontConfig;
    using NativeFont = Microsoft::Terminal::Settings::Model::Native::FontConfig;

    auto& fontAdapters()
    {
        static Microsoft::Terminal::Settings::Model::Adapters::AdapterCache<NativeFont, FontAdapter> cache;
        return cache;
    }
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    FontConfig::FontConfig(winrt::weak_ref<Model::Profile> sourceProfile) :
        _native{ Native::FontConfig::Create(Adapters::ToNative(sourceProfile)) }
    {
        fontAdapters().Register(_native.get(), this);
    }

    FontConfig::FontConfig(winrt::com_ptr<Native::FontConfig> native) :
        _native{ std::move(native) }
    {
    }

    FontConfig::~FontConfig()
    {
        fontAdapters().Remove(_native.get(), this);
    }

    winrt::com_ptr<FontConfig> FontConfig::FromNative(const winrt::com_ptr<Native::FontConfig>& native)
    {
        return fontAdapters().Get(native);
    }

    winrt::com_ptr<FontConfig> FontConfig::CopyFontInfo(const FontConfig* source, winrt::weak_ref<Model::Profile> sourceProfile)
    {
        return FromNative(Native::FontConfig::CopyFontInfo(source->_native.get(), Adapters::ToNative(sourceProfile)));
    }

    Json::Value FontConfig::ToJson() const
    {
        return _native->ToJson();
    }

    void FontConfig::LayerJson(const Json::Value& json)
    {
        _native->LayerJson(json);
    }
}

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    winrt::com_ptr<Native::FontConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::FontConfig& font)
    {
        return font ? winrt::get_self<FontAdapter>(font)->NativeModel() : nullptr;
    }

    winrt::Microsoft::Terminal::Settings::Model::FontConfig ToProjected(const winrt::com_ptr<Native::FontConfig>& font)
    {
        const auto adapter = FontAdapter::FromNative(font);
        if (adapter)
        {
            return *adapter;
        }
        return nullptr;
    }
}
