/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AppearanceConfig

Abstract:
- The implementation of the AppearanceConfig winrt class. Provides settings related
  to the appearance of the terminal, in both terminal control and terminal core.

Author(s):
- Pankaj Bhojwani - Nov 2020

--*/

#pragma once

#include "AppearanceConfig.g.h"
#include "ProfileAdapter.h"
#include "JsonUtils.h"
#include "IInheritable.h"
#include "MTSMSettings.h"
#include "MediaResourceSupport.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AppearanceConfig : AppearanceConfigT<AppearanceConfig, IMediaResourceContainer>
    {
    public:
        AppearanceConfig(winrt::weak_ref<Model::Profile> sourceProfile);
        explicit AppearanceConfig(winrt::com_ptr<Native::AppearanceConfig> native);
        ~AppearanceConfig();
        static winrt::com_ptr<AppearanceConfig> FromNative(const winrt::com_ptr<Native::AppearanceConfig>& native);
        const winrt::com_ptr<Native::AppearanceConfig>& NativeModel() const noexcept { return _native; }
        static winrt::com_ptr<AppearanceConfig> CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Model::Profile> sourceProfile);
        void ClearParents() { _native->ClearParents(); }
        void AddLeastImportantParent(const winrt::com_ptr<AppearanceConfig>& parent) { _native->AddLeastImportantParent(parent->_native); }
        void AddMostImportantParent(const winrt::com_ptr<AppearanceConfig>& parent) { _native->AddMostImportantParent(parent->_native); }
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);
        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const { _native->LogSettingChanges(changes, context); }

        Model::Profile SourceProfile() const { return Adapters::ToProjected(_native->SourceProfile()); }

        void ResolveMediaResources(const Model::MediaResourceResolver& resolver);

        TSM_ADAPTER_PROPERTY(std::optional<Native::Color>, Foreground);
        TSM_ADAPTER_PROPERTY(std::optional<Native::Color>, Background);
        TSM_ADAPTER_PROPERTY(std::optional<Native::Color>, SelectionBackground);
        TSM_ADAPTER_PROPERTY(std::optional<Native::Color>, CursorColor);
        TSM_ADAPTER_PROPERTY(float, Opacity);

        TSM_ADAPTER_PROPERTY(winrt::hstring, DarkColorSchemeName);
        TSM_ADAPTER_PROPERTY(winrt::hstring, LightColorSchemeName);

#define APPEARANCE_SETTINGS_INITIALIZE(type, name, ...) TSM_ADAPTER_PROPERTY(type, name);
        NATIVE_APPEARANCE_SETTINGS(APPEARANCE_SETTINGS_INITIALIZE)
#undef APPEARANCE_SETTINGS_INITIALIZE

    private:
        winrt::com_ptr<Native::AppearanceConfig> _native;
    };
}
