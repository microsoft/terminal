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
#include "JsonUtils.h"
#include "MTSMSettings.h"
#include "IInheritable.h"
#include <DefaultSettings.h>

using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct FontConfig : FontConfigT<FontConfig>, IInheritable<FontConfig>
    {
    public:
        FontConfig(winrt::weak_ref<Profile> sourceProfile);
        static winrt::com_ptr<FontConfig> CopyFontInfo(const FontConfig* source, winrt::weak_ref<Profile> sourceProfile);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

        Model::Profile SourceProfile();

#define FONT_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::FontConfig, type, name, ##__VA_ARGS__)
        MTSM_FONT_SETTINGS(FONT_SETTINGS_INITIALIZE)
#undef FONT_SETTINGS_INITIALIZE

    private:
        winrt::weak_ref<Profile> _sourceProfile;
    };
}
