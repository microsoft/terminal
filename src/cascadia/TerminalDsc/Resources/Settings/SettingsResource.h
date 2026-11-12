// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../Resource/DscResource.h"

namespace Microsoft::Terminal::Dsc
{
    // Manages Windows Terminal's global settings (the top-level scalars in
    // settings.json). Microsoft DSC resource type: Microsoft.WindowsTerminal/Settings
    class SettingsResource final : public IDscResource, public IGettable, public ISettable, public IExportable
    {
    public:
        const ResourceMetadata& Metadata() const noexcept override;
        Json::Value Schema() const override;

        Json::Value Get(const std::optional<Json::Value>& instance) override;
        Json::Value Set(const Json::Value& instance) override;
        std::vector<Json::Value> Export(const std::optional<Json::Value>& filter) override;

        // The disk-free core of Get/Set/Schema, split out for unit testing:
        // translate between the DSC property bag and a settings model instance.
        static Json::Value ReadState(const winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings& globals);
        static bool ApplyState(const winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings& globals, const Json::Value& desiredState);
        static Json::Value StateSchema();
    };
}
