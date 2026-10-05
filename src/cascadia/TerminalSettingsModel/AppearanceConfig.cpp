// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppearanceConfig.h"
#include "AppearanceConfig.g.cpp"
#include "AdapterCache.h"

namespace
{
    using AppearanceAdapter = winrt::Microsoft::Terminal::Settings::Model::implementation::AppearanceConfig;
    using NativeAppearance = Microsoft::Terminal::Settings::Model::Native::AppearanceConfig;

    auto& appearanceAdapters()
    {
        static Microsoft::Terminal::Settings::Model::Adapters::AdapterCache<NativeAppearance, AppearanceAdapter> cache;
        return cache;
    }
}

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    AppearanceConfig::AppearanceConfig(winrt::weak_ref<Model::Profile> sourceProfile) :
        _native{ Native::AppearanceConfig::Create(Adapters::ToNative(sourceProfile)) }
    {
        appearanceAdapters().Register(_native.get(), this);
    }

    AppearanceConfig::AppearanceConfig(winrt::com_ptr<Native::AppearanceConfig> native) :
        _native{ std::move(native) }
    {
    }

    AppearanceConfig::~AppearanceConfig()
    {
        appearanceAdapters().Remove(_native.get(), this);
    }

    winrt::com_ptr<AppearanceConfig> AppearanceConfig::FromNative(const winrt::com_ptr<Native::AppearanceConfig>& native)
    {
        return appearanceAdapters().Get(native);
    }

    winrt::com_ptr<AppearanceConfig> AppearanceConfig::CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Model::Profile> sourceProfile)
    {
        return FromNative(Native::AppearanceConfig::CopyAppearance(source->_native.get(), Adapters::ToNative(sourceProfile)));
    }

    Json::Value AppearanceConfig::ToJson() const
    {
        return _native->ToJson();
    }

    void AppearanceConfig::LayerJson(const Json::Value& json)
    {
        _native->LayerJson(json);
    }

    void AppearanceConfig::ResolveMediaResources(const Model::MediaResourceResolver& resolver)
    {
        _native->ResolveMediaResources([&](const Native::OriginTag origin, const winrt::hstring& path, const winrt::com_ptr<Native::MediaResource>& resource) {
            resolver(Adapters::ToProjected(origin), path, Adapters::ToProjected(resource));
        });
    }
}

namespace Microsoft::Terminal::Settings::Model::Adapters
{
    winrt::com_ptr<Native::AppearanceConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig& appearance)
    {
        return appearance ? winrt::get_self<AppearanceAdapter>(appearance)->NativeModel() : nullptr;
    }

    winrt::com_ptr<Native::AppearanceConfig> ToNative(const winrt::Microsoft::Terminal::Settings::Model::AppearanceConfig& appearance)
    {
        return appearance ? winrt::get_self<AppearanceAdapter>(appearance)->NativeModel() : nullptr;
    }

    winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig ToProjected(const winrt::com_ptr<Native::AppearanceConfig>& appearance)
    {
        const auto adapter = AppearanceAdapter::FromNative(appearance);
        if (adapter)
        {
            return *adapter;
        }
        return nullptr;
    }
}
