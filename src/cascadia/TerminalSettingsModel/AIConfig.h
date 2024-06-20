/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AIConfig

Abstract:
- The implementation of the AIConfig winrt class. Provides settings related
  to the AI settings of the terminal

Author(s):
- Pankaj Bhojwani - June 2024

--*/

#pragma once

#include "pch.h"
#include "AIConfig.g.h"
#include "IInheritable.h"
#include "JsonUtils.h"
#include "MTSMSettings.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AIConfig : AIConfigT<AIConfig>, IInheritable<AIConfig>
    {
    public:
        AIConfig() = default;
        static winrt::com_ptr<AIConfig> CopyAIConfig(const AIConfig* source);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

#define AI_SETTINGS_INITIALIZE(type, name, jsonKey, ...) \
    INHERITABLE_SETTING(Model::AIConfig, type, name, ##__VA_ARGS__)
        MTSM_AI_SETTINGS(AI_SETTINGS_INITIALIZE)
#undef AI_SETTINGS_INITIALIZE
    };
}
