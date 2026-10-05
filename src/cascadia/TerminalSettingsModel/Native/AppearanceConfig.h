// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profile.h"
#include <tuple>

namespace Microsoft::Terminal::Settings::Model::Native
{
    class AppearanceConfig : public winrt::implements<AppearanceConfig, winrt::Windows::Foundation::IInspectable>
    {
        class ConstructionToken
        {
        public:
            ConstructionToken(const ConstructionToken&) = default;

        private:
            ConstructionToken() = default;
            friend class AppearanceConfig;
        };

    public:
        TSM_NATIVE_LIFETIME_DECLARATIONS(AppearanceConfig);
        TSM_NATIVE_API static winrt::com_ptr<AppearanceConfig> Create(winrt::weak_ref<Profile> sourceProfile);
        AppearanceConfig(ConstructionToken, winrt::weak_ref<Profile> sourceProfile);
        TSM_NATIVE_API ~AppearanceConfig() noexcept;

        TSM_NATIVE_API void ClearParents();
        TSM_NATIVE_API void AddLeastImportantParent(winrt::com_ptr<AppearanceConfig> parent);
        TSM_NATIVE_API void AddMostImportantParent(winrt::com_ptr<AppearanceConfig> parent);
        TSM_NATIVE_API std::span<const winrt::com_ptr<AppearanceConfig>> Parents() const noexcept;
        TSM_NATIVE_API static winrt::com_ptr<AppearanceConfig> CopyAppearance(const AppearanceConfig* source, winrt::weak_ref<Profile> sourceProfile);
        TSM_NATIVE_API winrt::com_ptr<Profile> SourceProfile() const;
        TSM_NATIVE_API Json::Value ToJson() const;
        TSM_NATIVE_API void LayerJson(const Json::Value& json);
        TSM_NATIVE_API void LogSettingChanges(std::set<std::string>& changes, const std::string_view& context) const;
        TSM_NATIVE_API void ResolveMediaResources(const MediaResourceResolver& resolver);

        TSM_NATIVE_PROPERTY(AppearanceConfig, std::optional<Native::Color>, Foreground);
        TSM_NATIVE_PROPERTY(AppearanceConfig, std::optional<Native::Color>, Background);
        TSM_NATIVE_PROPERTY(AppearanceConfig, std::optional<Native::Color>, SelectionBackground);
        TSM_NATIVE_PROPERTY(AppearanceConfig, std::optional<Native::Color>, CursorColor);
        TSM_NATIVE_PROPERTY(AppearanceConfig, float, Opacity);
        TSM_NATIVE_PROPERTY(AppearanceConfig, winrt::hstring, DarkColorSchemeName);
        TSM_NATIVE_PROPERTY(AppearanceConfig, winrt::hstring, LightColorSchemeName);

#define DECLARE_NATIVE_APPEARANCE_PROPERTY(type, name, ...) TSM_NATIVE_PROPERTY(AppearanceConfig, type, name);
        NATIVE_APPEARANCE_SETTINGS(DECLARE_NATIVE_APPEARANCE_PROPERTY)
#undef DECLARE_NATIVE_APPEARANCE_PROPERTY

    private:
        using base_type = winrt::implements<AppearanceConfig, winrt::Windows::Foundation::IInspectable>;

        void _logSettingSet(const std::string_view& setting);
        void _logSettingIfSet(const std::string_view& setting, bool isSet);
        std::tuple<winrt::hstring, OriginTag> _getSourceProfileBasePathAndOrigin() const;

        std::vector<winrt::com_ptr<AppearanceConfig>> _parents;
        winrt::weak_ref<Profile> _sourceProfile;
        std::set<std::string> _changeLog;
    };
}
