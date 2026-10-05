/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontConfig

Abstract:
- The implementation of the FontConfig winrt class. Provides settings related
  to the font settings of the terminal, for the terminal control.

Author(s):
- Pankaj Bhojwani - June 2021

--*/

#pragma once

#include "pch.h"
#include "FontConfig.g.h"
#include "ProfileAdapter.h"
#include "JsonUtils.h"
#include "MTSMSettings.h"
#include "IInheritable.h"
#include <DefaultSettings.h>

using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct FontConfig : FontConfigT<FontConfig>
    {
    public:
        FontConfig(winrt::weak_ref<Model::Profile> sourceProfile);
        explicit FontConfig(winrt::com_ptr<Native::FontConfig> native);
        ~FontConfig();
        static winrt::com_ptr<FontConfig> FromNative(const winrt::com_ptr<Native::FontConfig>& native);
        const winrt::com_ptr<Native::FontConfig>& NativeModel() const noexcept { return _native; }
        static winrt::com_ptr<FontConfig> CopyFontInfo(const FontConfig* source, winrt::weak_ref<Model::Profile> sourceProfile);
        void ClearParents() { _native->ClearParents(); }
        void AddLeastImportantParent(const winrt::com_ptr<FontConfig>& parent) { _native->AddLeastImportantParent(parent->_native); }
        void AddMostImportantParent(const winrt::com_ptr<FontConfig>& parent) { _native->AddMostImportantParent(parent->_native); }
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);
        void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const { _native->LogSettingChanges(changes, context); }

        Model::Profile SourceProfile() const { return Adapters::ToProjected(_native->SourceProfile()); }

#define FONT_SETTINGS_INITIALIZE(type, name, ...) TSM_ADAPTER_PROPERTY(type, name);
        NATIVE_FONT_SETTINGS(FONT_SETTINGS_INITIALIZE)
#undef FONT_SETTINGS_INITIALIZE

    private:
        winrt::com_ptr<Native::FontConfig> _native;
    };
}
