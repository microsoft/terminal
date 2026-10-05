// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profile.h"

namespace Microsoft::Terminal::Settings::Model::Native
{
    class FontConfig : public winrt::implements<FontConfig, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class FontConfig;
        };

    public:
        TSM_NATIVE_LIFETIME_DECLARATIONS(FontConfig);
        TSM_NATIVE_API static winrt::com_ptr<FontConfig> Create(winrt::weak_ref<Profile> sourceProfile);
        FontConfig(ConstructionToken, winrt::weak_ref<Profile> sourceProfile);
        TSM_NATIVE_API ~FontConfig() noexcept;

        TSM_NATIVE_API void ClearParents();
        TSM_NATIVE_API void AddLeastImportantParent(winrt::com_ptr<FontConfig> parent);
        TSM_NATIVE_API void AddMostImportantParent(winrt::com_ptr<FontConfig> parent);
        TSM_NATIVE_API std::span<const winrt::com_ptr<FontConfig>> Parents() const noexcept;
        TSM_NATIVE_API static winrt::com_ptr<FontConfig> CopyFontInfo(const FontConfig* source, winrt::weak_ref<Profile> sourceProfile);
        TSM_NATIVE_API winrt::com_ptr<Profile> SourceProfile() const;
        TSM_NATIVE_API Json::Value ToJson() const;
        TSM_NATIVE_API void LayerJson(const Json::Value& json);
        TSM_NATIVE_API void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;

#define DECLARE_NATIVE_FONT_PROPERTY(type, name, ...) TSM_NATIVE_PROPERTY(FontConfig, type, name);
        NATIVE_FONT_SETTINGS(DECLARE_NATIVE_FONT_PROPERTY)
#undef DECLARE_NATIVE_FONT_PROPERTY

    private:
        using base_type = winrt::implements<FontConfig, winrt::Windows::Foundation::IInspectable>;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, bool isSet);

        std::vector<winrt::com_ptr<FontConfig>> _parents;
        winrt::weak_ref<Profile> _sourceProfile;
        std::set<std::string> _changeLog;
    };
}
