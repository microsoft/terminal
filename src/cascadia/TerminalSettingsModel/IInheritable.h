/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IInheritable.h

Abstract:
- An interface allowing settings objects to inherit settings from a parent

Author(s):
- Carlos Zamora - October 2020

--*/
#pragma once

#include "JsonUtils.h"
#include "JsonSyncCollections.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    template<typename T>
    struct IInheritable
    {
    public:
        // Method Description:
        // - Create a new instance of T, but set its parent to this instance
        // Arguments:
        // - <none>
        // Return Value:
        // - a new instance of T with this instance set as its parent
        com_ptr<T> CreateChild() const
        {
            auto child{ winrt::make_self<T>() };

            // set "this" as the parent.
            // However, "this" is an IInheritable, so we need to cast it as T (the impl winrt type)
            //  to pass ownership over to the com_ptr.
            com_ptr<T> parent;
            winrt::copy_from_abi(parent, const_cast<T*>(static_cast<const T*>(this)));
            child->AddLeastImportantParent(parent);

            child->_FinalizeInheritance();
            return child;
        }

        void ClearParents()
        {
            _parents.clear();
        }

        void AddLeastImportantParent(com_ptr<T> parent)
        {
            _parents.emplace_back(std::move(parent));
        }

        void AddMostImportantParent(com_ptr<T> parent)
        {
            _parents.emplace(_parents.begin(), std::move(parent));
        }

        const std::vector<com_ptr<T>>& Parents()
        {
            return _parents;
        }

    protected:
        std::vector<com_ptr<T>> _parents{};

        // Method Description:
        // - Actions to be performed after a child was created. Generally used to set
        //   any extraneous data from the parent into the child.
        virtual void _FinalizeInheritance() {}

        // Method Description:
        // - Eagerly deserialize every JSON-backed setting in *this layer* so malformed
        //   values throw a JsonUtils::DeserializationError at parse time (reported with
        //   location info by the settings loader) instead of lazily at first getter
        //   access. Each settings class overrides this to validate its own settings.
        virtual void _ValidateThisLayer() const {}
    };

    // This is like std::optional, but we can use it in inheritance to determine whether the user explicitly cleared it
    template<typename T>
    using NullableSetting = std::optional<std::optional<T>>;
}

// =============================================================================
// Inheritable setting macros
// =============================================================================
//
// These macros implement the standard functions for inheritable settings:
//   - Has<NAME>(): checks if the user explicitly set a value for this setting
//   - <NAME>OverrideSource(): returns the object that provides the resolved value
//   - Getter(): returns the resolved value (this layer --> parent --> default)
//   - Setter(): sets the value
//   - Clear<NAME>(): clears the user set value (reverts to inherited/default)
//
// Storage strategies (in order of preference):
//
//   1. JSON-backed scalars (the default for new settings). State lives in
//      _json (a Json::Value member). Use these for any setting with a
//      ConversionTrait. The owning class must declare
//      `Json::Value _json{ Json::ValueType::objectValue };`.
//      Macros: INHERITABLE_SETTING, INHERITABLE_SETTING_WITH_LOGGING,
//              INHERITABLE_NULLABLE_SETTING.
//
//   2. JSON-backed collections (IVector<T> / IMap<K,V>). The getter returns
//      a JsonSyncVector / JsonSyncMap wrapper that mirrors every in-place
//      mutation into _json[jsonKey], so .Append / .InsertAt / .Insert /
//      .RemoveAt / etc. don't get lost. _json is the only persistent state.
//      Macros: INHERITABLE_JSON_BACKED_VECTOR_SETTING,
//              INHERITABLE_JSON_BACKED_MAP_SETTING.
//
//   3. Backing-field (narrow special case for runtime resolution caches and
//      sub-object presence/absence markers). State lives in a
//      std::optional<T> member. Used when the runtime needs a persistent
//      cache that JSON does not carry — specifically:
//        - IMediaResource scalars: Icon, PixelShaderPath, BackgroundImagePath,
//          PixelShaderImagePath. _resolved state set by ResolveMediaResources
//          must persist across getter calls. (INHERITABLE_MEDIA_RESOURCE_SETTING.)
//        - Sub-object presence markers: UnfocusedAppearance.
//          (INHERITABLE_MUTABLE_SETTING.)
//        - Hand-written hybrid cases: Profile::BellSound,
//          GlobalAppSettings::NewTabMenu. Both keep a backing field for nested
//          IMediaResource resolution but dual-write to _json via either a
//          JsonSyncVector wrapper getter (BellSound) or surgical mutation APIs
//          (NewTabMenu's Insert/Remove/Set/...InFolder/UpdateFolder*).
//
// When adding a new setting, prefer (1) for scalars and (2) for collections.
// Reach for (3) only when the runtime needs state that JSON doesn't capture.
//
// Available macros (JSON-backed scalars):
//   INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)
//   INHERITABLE_SETTING_WITH_LOGGING(projectedType, type, name, jsonKey, ...)
//   INHERITABLE_NULLABLE_SETTING(projectedType, type, name, jsonKey, ...)
//   _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)
//
// Available macros (JSON-backed collections):
//   INHERITABLE_JSON_BACKED_VECTOR_SETTING(projectedType, type, name, jsonKey, ...)
//   INHERITABLE_JSON_BACKED_MAP_SETTING(projectedType, type, name, jsonKey, ...)
//
// Available macros (backing-field):
//   INHERITABLE_MUTABLE_SETTING(projectedType, type, name, ...)
//   INHERITABLE_MEDIA_RESOURCE_SETTING(projectedType, name, jsonKey, ...)
//
// =============================================================================

// =============================================================================
// JSON-backed inheritable settings
// =============================================================================
// No std::optional<T> backing field — _json is the source of truth.
// Getters deserialize from _json via ConversionTrait on each call.
// Setters serialize to _json via SetValueForKey.
// Has/Clear check/modify _json directly.

// Shared base for JSON-backed inheritable settings.
// Provides Has, Clear, OverrideSource, and the private implementation helpers.
// Does NOT provide the public getter/setter — use INHERITABLE_SETTING or
// INHERITABLE_SETTING_WITH_LOGGING which add those on top of this base.
#define _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)     \
public:                                                                        \
    /* Returns true if the user explicitly set the value, false otherwise */   \
    bool Has##name() const                                                     \
    {                                                                          \
        return _json.isMember(jsonKey) && !_json[jsonKey].isNull();            \
    }                                                                          \
                                                                               \
    /* Returns the object that provides the resolved value */                  \
    projectedType name##OverrideSource()                                       \
    {                                                                          \
        /*iterate through parents to find one with a value*/                   \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return *source;                                                \
            }                                                                  \
        }                                                                      \
                                                                               \
        /*no source was found*/                                                \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    /* Clear the user set value */                                             \
    void Clear##name()                                                         \
    {                                                                          \
        _json.removeMember(JsonKey(jsonKey));                                  \
    }                                                                          \
                                                                               \
private:                                                                       \
    /* Read value from this layer's _json only (no parent walk) */             \
    std::optional<type> _get##name##FromThisLayer() const                      \
    {                                                                          \
        if (Has##name())                                                       \
        {                                                                      \
            type val{ __VA_ARGS__ };                                           \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey( \
                _json, jsonKey, val);                                          \
            return val;                                                        \
        }                                                                      \
        return std::nullopt;                                                   \
    }                                                                          \
                                                                               \
    /* Resolve the value: this layer --> parents --> nullopt */                \
    std::optional<type> _get##name##Impl() const                               \
    {                                                                          \
        /*return value from this layer*/                                       \
        if (auto val{ _get##name##FromThisLayer() })                           \
        {                                                                      \
            return val;                                                        \
        }                                                                      \
                                                                               \
        /*iterate through parents to find a value*/                            \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto val{ parent->_get##name##Impl() })                        \
            {                                                                  \
                return val;                                                    \
            }                                                                  \
        }                                                                      \
                                                                               \
        /*no value was found*/                                                 \
        return std::nullopt;                                                   \
    }                                                                          \
                                                                               \
    /* Find the object that provides the resolved value */                     \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())              \
    {                                                                          \
        /*we have a value*/                                                    \
        if (Has##name())                                                       \
        {                                                                      \
            return get_strong();                                               \
        }                                                                      \
                                                                               \
        /*iterate through parents to find one with a value*/                   \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return source;                                                 \
            }                                                                  \
        }                                                                      \
                                                                               \
        /*no value was found*/                                                 \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    /* Find the override source and the value it provides */                   \
    auto _get##name##OverrideSourceAndValueImpl()                              \
        ->std::pair<decltype(get_strong()), std::optional<type>>               \
    {                                                                          \
        /*we have a value*/                                                    \
        if (Has##name())                                                       \
        {                                                                      \
            return { get_strong(), _get##name##FromThisLayer() };              \
        }                                                                      \
                                                                               \
        /*iterate through parents to find one with a value*/                   \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return { source, source->_get##name##FromThisLayer() };        \
            }                                                                  \
        }                                                                      \
                                                                               \
        /*no value was found*/                                                 \
        return { nullptr, std::nullopt };                                      \
    }

// JSON-backed inheritable setting.
// Getter returns the resolved value: this layer --> inherited --> default.
// Setter writes the value to _json.
#define INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)           \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__) \
public:                                                                        \
    /* Returns the resolved value for this setting */                          \
    /* fallback: this layer --> inherited value --> default */                 \
    type name() const                                                          \
    {                                                                          \
        const auto val{ _get##name##Impl() };                                  \
        return val ? *val : type{ __VA_ARGS__ };                               \
    }                                                                          \
                                                                               \
    /* Overwrite the user set value in _json */                                \
    void name(const type& value)                                               \
    {                                                                          \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(     \
            _json, jsonKey, value);                                            \
    }

// JSON-backed inheritable setting with change logging.
// Same as INHERITABLE_SETTING, but the setter also calls _logSettingSet(jsonKey)
// when the value actually changes (for settings change telemetry).
#define INHERITABLE_SETTING_WITH_LOGGING(projectedType, type, name, jsonKey, ...) \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)    \
public:                                                                           \
    /* Returns the resolved value for this setting */                             \
    /* fallback: this layer --> inherited value --> default */                    \
    type name() const                                                             \
    {                                                                             \
        const auto val{ _get##name##Impl() };                                     \
        return val ? *val : type{ __VA_ARGS__ };                                  \
    }                                                                             \
                                                                                  \
    /* Overwrite the user set value, log the change, and write to _json */        \
    void name(const type& value)                                                  \
    {                                                                             \
        const auto existingVal{ _get##name##FromThisLayer() };                    \
        if (!existingVal.has_value() || existingVal.value() != value)             \
        {                                                                         \
            _logSettingSet(jsonKey);                                              \
        }                                                                         \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(        \
            _json, jsonKey, value);                                               \
    }

// JSON-backed nullable inheritable setting.
// For settings where null is a valid explicit value (e.g., Foreground color).
//   Key absent  → inherit from parent
//   Key present + null  → explicitly "no value" (getter returns nullptr)
//   Key present + value → has a value
// Getter returns IReference<T>.
#define INHERITABLE_NULLABLE_SETTING(projectedType, type, name, jsonKey, ...)  \
public:                                                                        \
    /* Key presence means explicitly set (even if value is null) */            \
    bool Has##name() const                                                     \
    {                                                                          \
        return _json.isMember(jsonKey);                                        \
    }                                                                          \
                                                                               \
    /* Returns the object that provides the resolved value */                  \
    projectedType name##OverrideSource()                                       \
    {                                                                          \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return *source;                                                \
            }                                                                  \
        }                                                                      \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    /* Clear the user set value (remove key entirely → inherit from parent) */ \
    void Clear##name()                                                         \
    {                                                                          \
        _json.removeMember(JsonKey(jsonKey));                                  \
    }                                                                          \
                                                                               \
    /* Returns the resolved value for this setting */                          \
    /* fallback: this layer --> inherited value --> default */                 \
    winrt::Windows::Foundation::IReference<type> name() const                  \
    {                                                                          \
        const auto val{ _get##name##Impl() };                                  \
        if (val)                                                               \
        {                                                                      \
            if (*val)                                                          \
            {                                                                  \
                return **val;                                                  \
            }                                                                  \
            return nullptr;                                                    \
        }                                                                      \
        return winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ };    \
    }                                                                          \
                                                                               \
    /* Overwrite the user set value in _json */                                \
    void name(const winrt::Windows::Foundation::IReference<type>& value)       \
    {                                                                          \
        if (value)                                                             \
        {                                                                      \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey( \
                _json, jsonKey, value.Value());                                \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            /* explicitly set to null (not the same as clearing) */            \
            _json[JsonKey(jsonKey)] = Json::nullValue;                         \
        }                                                                      \
    }                                                                          \
                                                                               \
private:                                                                       \
    /* Read nullable value from this layer's _json only (no parent walk) */    \
    /* Returns: nullopt = not set. optional{nullopt} = explicitly null. */     \
    /*          optional{optional{val}} = has value. */                        \
    NullableSetting<type> _get##name##FromThisLayer() const                    \
    {                                                                          \
        if (_json.isMember(jsonKey))                                           \
        {                                                                      \
            if (_json[jsonKey].isNull())                                       \
            {                                                                  \
                return std::optional<type>{ std::nullopt };                    \
            }                                                                  \
            type val{};                                                        \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey( \
                _json, jsonKey, val);                                          \
            return std::optional<type>{ val };                                 \
        }                                                                      \
        return std::nullopt;                                                   \
    }                                                                          \
                                                                               \
    /* Resolve the nullable value: this layer --> parents --> nullopt */       \
    NullableSetting<type> _get##name##Impl() const                             \
    {                                                                          \
        if (auto val{ _get##name##FromThisLayer() })                           \
        {                                                                      \
            return val;                                                        \
        }                                                                      \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto val{ parent->_get##name##Impl() })                        \
            {                                                                  \
                return val;                                                    \
            }                                                                  \
        }                                                                      \
        return std::nullopt;                                                   \
    }                                                                          \
                                                                               \
    /* Find the object that provides the resolved value */                     \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())              \
    {                                                                          \
        if (Has##name())                                                       \
        {                                                                      \
            return get_strong();                                               \
        }                                                                      \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return source;                                                 \
            }                                                                  \
        }                                                                      \
        return nullptr;                                                        \
    }                                                                          \
                                                                               \
    /* Find the override source and the value it provides */                   \
    auto _get##name##OverrideSourceAndValueImpl()                              \
        ->std::pair<decltype(get_strong()), NullableSetting<type>>             \
    {                                                                          \
        if (Has##name())                                                       \
        {                                                                      \
            return { get_strong(), _get##name##FromThisLayer() };              \
        }                                                                      \
        for (const auto& parent : _parents)                                    \
        {                                                                      \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })       \
            {                                                                  \
                return { source, source->_get##name##FromThisLayer() };        \
            }                                                                  \
        }                                                                      \
        return { nullptr, std::nullopt };                                      \
    }

// =============================================================================
// Mutable backing-field inheritable settings
// =============================================================================
// These use a std::optional<T> backing field instead of _json.
//
// Use these ONLY when runtime state must persist across getter calls and JSON
// alone cannot capture that state — specifically IMediaResource resolution
// state and sub-object presence markers.
//
// Current direct uses (after the auto-save refactor):
//   Profile:           UnfocusedAppearance (INHERITABLE_MUTABLE_SETTING),
//                      Icon (INHERITABLE_MEDIA_RESOURCE_SETTING),
//                      BellSound (hand-written: backing field + wrapper getter)
//   GlobalAppSettings: NewTabMenu (hand-written: backing field + surgical APIs)
//   AppearanceConfig:  PixelShaderPath, PixelShaderImagePath, BackgroundImagePath
//                      (INHERITABLE_MEDIA_RESOURCE_SETTING)
//
// Collection settings (DisabledProfileSources, FontAxes, FontFeatures,
// EnvironmentVariables) are now JSON-backed via
// INHERITABLE_JSON_BACKED_VECTOR_SETTING / _MAP_SETTING.

// Shared base for backing-field inheritable settings.
// Provides Has, OverrideSource, and the private implementation helpers
// (backing field + parent-walk resolvers).
//
// Each OUTER macro that uses this base is responsible for defining its own
// Clear<Name>() — backing-only for plain INHERITABLE_MUTABLE_SETTING, dual-write
// (backing + _json) for INHERITABLE_MEDIA_RESOURCE_SETTING and
// INHERITABLE_MEDIA_RESOURCE_VECTOR_SETTING — and its own getter/setter.
#define _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, storageType, name, ...) \
public:                                                                          \
    /* Returns true if the user explicitly set the value, false otherwise */     \
    bool Has##name() const                                                       \
    {                                                                            \
        return _##name.has_value();                                              \
    }                                                                            \
                                                                                 \
    /* Returns the object that provides the resolved value */                    \
    projectedType name##OverrideSource()                                         \
    {                                                                            \
        /*iterate through parents to find one with a value*/                     \
        for (const auto& parent : _parents)                                      \
        {                                                                        \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })         \
            {                                                                    \
                return *source;                                                  \
            }                                                                    \
        }                                                                        \
                                                                                 \
        /*no source was found*/                                                  \
        return nullptr;                                                          \
    }                                                                            \
                                                                                 \
private:                                                                         \
    storageType _##name{ std::nullopt };                                         \
                                                                                 \
    /* Resolve the value: this layer --> parents --> nullopt */                  \
    storageType _get##name##Impl() const                                         \
    {                                                                            \
        /*return user set value*/                                                \
        if (_##name)                                                             \
        {                                                                        \
            return _##name;                                                      \
        }                                                                        \
                                                                                 \
        /*iterate through parents to find a value*/                              \
        for (const auto& parent : _parents)                                      \
        {                                                                        \
            if (auto val{ parent->_get##name##Impl() })                          \
            {                                                                    \
                return val;                                                      \
            }                                                                    \
        }                                                                        \
                                                                                 \
        /*no value was found*/                                                   \
        return std::nullopt;                                                     \
    }                                                                            \
                                                                                 \
    /* Find the object that provides the resolved value */                       \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())                \
    {                                                                            \
        /*we have a value*/                                                      \
        if (_##name)                                                             \
        {                                                                        \
            return get_strong();                                                 \
        }                                                                        \
                                                                                 \
        /*iterate through parents to find one with a value*/                     \
        for (const auto& parent : _parents)                                      \
        {                                                                        \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })         \
            {                                                                    \
                return source;                                                   \
            }                                                                    \
        }                                                                        \
                                                                                 \
        /*no value was found*/                                                   \
        return nullptr;                                                          \
    }                                                                            \
                                                                                 \
    /* Find the override source and the value it provides */                     \
    auto _get##name##OverrideSourceAndValueImpl()                                \
        ->std::pair<decltype(get_strong()), storageType>                         \
    {                                                                            \
        /*we have a value*/                                                      \
        if (_##name)                                                             \
        {                                                                        \
            return { get_strong(), _##name };                                    \
        }                                                                        \
                                                                                 \
        /*iterate through parents to find one with a value*/                     \
        for (const auto& parent : _parents)                                      \
        {                                                                        \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })         \
            {                                                                    \
                return { source, source->_##name };                              \
            }                                                                    \
        }                                                                        \
                                                                                 \
        /*no value was found*/                                                   \
        return { nullptr, std::nullopt };                                        \
    }

// Mutable backing-field setting.
// Getter returns the resolved value: this layer --> inherited --> default.
// Setter writes to the std::optional<T> backing field.
// Clear<Name>() resets the backing field only (this macro has no JSON to clear).
#define INHERITABLE_MUTABLE_SETTING(projectedType, type, name, ...)                  \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, std::optional<type>, name, ...) \
public:                                                                              \
    /* Returns the resolved value for this setting */                                \
    /* fallback: this layer --> inherited value --> default */                       \
    type name() const                                                                \
    {                                                                                \
        const auto val{ _get##name##Impl() };                                        \
        return val ? *val : type{ __VA_ARGS__ };                                     \
    }                                                                                \
                                                                                     \
    /* Overwrite the user set value */                                               \
    void name(const type& value)                                                     \
    {                                                                                \
        _##name = value;                                                             \
    }                                                                                \
                                                                                     \
    /* Clear the user set value (backing only — no _json key for this macro) */      \
    void Clear##name()                                                               \
    {                                                                                \
        _##name = std::nullopt;                                                      \
    }

// =============================================================================
// IMediaResource settings with backing fields for resolution lifecycle.
// =============================================================================
//
// Use for settings of type IMediaResource that need in-place resolution
// (e.g., Icon, PixelShaderPath, BackgroundImagePath). The backing field
// holds the runtime-resolved object. _json is the source of truth for
// serialization. The setter and Clear dual-write to both.
//
// Parameters:
//   projectedType - the WinRT projected type (e.g., Model::Profile)
//   name          - the setting name (e.g., Icon)
//   jsonKey       - the JSON key (e.g., "icon")
//   ...           - default value (e.g., implementation::MediaResource::Empty())
//
#define INHERITABLE_MEDIA_RESOURCE_SETTING(projectedType, name, jsonKey, ...)                                                     \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, std::optional<IMediaResource>, name, implementation::MediaResource::Empty()) \
public:                                                                                                                           \
    /* Returns the resolved value: this layer --> inherited --> default */                                                        \
    IMediaResource name() const                                                                                                   \
    {                                                                                                                             \
        const auto v{ _get##name##Impl() };                                                                                       \
        return v ? *v : IMediaResource{ __VA_ARGS__ };                                                                            \
    }                                                                                                                             \
                                                                                                                                  \
    /* Dual-write: backing field (for resolution) + _json (for serialization) */                                                  \
    void name(const IMediaResource& value)                                                                                        \
    {                                                                                                                             \
        _##name = value;                                                                                                          \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(_json, jsonKey, value);                                 \
        _logSettingSet(jsonKey);                                                                                                  \
        /* TODO GH#12424: raise WriteSettings event */                                                                            \
    }                                                                                                                             \
                                                                                                                                  \
    /* Dual-clear: backing field + _json key. Both must clear so auto-save doesn't */                                             \
    /* re-persist the cleared value. */                                                                                           \
    void Clear##name()                                                                                                            \
    {                                                                                                                             \
        _##name = std::nullopt;                                                                                                   \
        _json.removeMember(JsonKey(jsonKey));                                                                                     \
        _logSettingSet(jsonKey);                                                                                                  \
        /* TODO GH#12424: raise WriteSettings event */                                                                            \
    }

// =============================================================================
// IMediaResource vector settings (BellSound-style).
// =============================================================================
//
// Use for settings of type IVector<IMediaResource> that need in-place
// resolution AND in-place IVector mutation (.Append, .InsertAt, .RemoveAt, …).
// The backing field holds the runtime-resolved IMediaResource instances.
// _json is the source of truth for serialization. The setter, Clear, and
// in-place wrapper mutations all dual-write to both backing and _json.
//
// Asymmetric getter:
//   - When Has<Name>() (this layer has its own value): returns a JsonSyncVector
//     wrapper over the backing field. Mutations on the wrapper dual-write to
//     both _##name and _json[jsonKey] via the onChange callback.
//   - When inherited from a parent: returns the parent's IVector directly (no
//     wrapper). Callers MUST materialize a local copy before mutating, by
//     calling the whole-replace setter (the editor handles this in
//     ProfileViewModel::_PrepareModelForBellSoundModification). After that,
//     subsequent getter calls return a wrapper over the now-local backing.
//
// This asymmetry preserves parent isolation — child mutations through the
// wrapper write into the child's own _##name and _json, never the parent's.
//
// Parameters:
//   projectedType - the WinRT projected type (e.g., Model::Profile)
//   name          - the setting name (e.g., BellSound)
//   jsonKey       - the JSON key (e.g., "bellSound")
//   ...           - fallback value when no layer has a value (e.g., nullptr).
//                   Wrapped as IVector<IMediaResource>{ __VA_ARGS__ }, so
//                   accepts nullptr or any IVector<IMediaResource>{}-compatible
//                   initializer.
//
// Requires JsonSyncCollections.h to be transitively included.
#define INHERITABLE_MEDIA_RESOURCE_VECTOR_SETTING(projectedType, name, jsonKey, ...)                                           \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType,                                                                           \
                                      std::optional<winrt::Windows::Foundation::Collections::IVector<IMediaResource>>,         \
                                      name,                                                                                    \
                                      __VA_ARGS__)                                                                             \
public:                                                                                                                        \
    /* Asymmetric getter: wrapper when local, raw inherited IVector otherwise. */                                              \
    /* Returns the fallback default (typically nullptr) when no effective value exists. */                                     \
    winrt::Windows::Foundation::Collections::IVector<IMediaResource> name()                                                    \
    {                                                                                                                          \
        const auto val{ _get##name##Impl() };                                                                                  \
        if (!val || !*val)                                                                                                     \
        {                                                                                                                      \
            return winrt::Windows::Foundation::Collections::IVector<IMediaResource>{ __VA_ARGS__ };                            \
        }                                                                                                                      \
        if (Has##name())                                                                                                       \
        {                                                                                                                      \
            auto strong = get_strong();                                                                                        \
            return winrt::make<::winrt::Microsoft::Terminal::Settings::Model::implementation::JsonSyncVector<IMediaResource>>( \
                *val,                                                                                                          \
                [strong](winrt::Windows::Foundation::Collections::IVector<IMediaResource> const& current) {                    \
                    auto temp = ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<                            \
                                    winrt::Windows::Foundation::Collections::IVector<IMediaResource>>{}                        \
                                    .ToJson(current);                                                                          \
                    strong->_json[JsonKey(jsonKey)] = std::move(temp);                                                         \
                    strong->_logSettingSet(jsonKey);                                                                           \
                    /* TODO GH#12424: raise WriteSettings event */                                                             \
                });                                                                                                            \
        }                                                                                                                      \
        return *val;                                                                                                           \
    }                                                                                                                          \
                                                                                                                               \
    /* Whole-replace setter: dual-write to backing + _json. */                                                                 \
    void name(const winrt::Windows::Foundation::Collections::IVector<IMediaResource>& value)                                   \
    {                                                                                                                          \
        _##name = value;                                                                                                       \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(_json, jsonKey, value);                              \
        _logSettingSet(jsonKey);                                                                                               \
        /* TODO GH#12424: raise WriteSettings event */                                                                         \
    }                                                                                                                          \
                                                                                                                               \
    /* Dual-clear: backing field + _json key. */                                                                               \
    void Clear##name()                                                                                                         \
    {                                                                                                                          \
        _##name = std::nullopt;                                                                                                \
        _json.removeMember(JsonKey(jsonKey));                                                                                  \
        _logSettingSet(jsonKey);                                                                                               \
        /* TODO GH#12424: raise WriteSettings event */                                                                         \
    }

// =============================================================================
// JSON-backed collection inheritable settings (wrapper-returning getter)
// =============================================================================
//
// Use these for settings whose runtime type is IVector<T> / IMap<K,V> and that
// callers mutate in place (.Append, .InsertAt, .RemoveAt, .Insert, .Remove, …).
// The getter returns a JsonSyncVector<T> / JsonSyncMap<K,V> wrapper that:
//   - delegates reads to a shadow seeded from this layer's effective JSON,
//   - mirrors every in-place mutation back into _json[jsonKey] via the wrapper's
//     onChange callback,
//   - returns nullptr when no effective value exists (preserves existing
//     null-check semantics in caller sites that distinguish "absent" from
//     "empty collection" and switch to the whole-replace setter path).
//
// Mutations through the returned wrapper auto-promote the setting into this
// layer's _json (the wrapper's shadow is independent of any parent state, so
// the parent is never mutated by the child).
//
// Requires JsonSyncCollections.h to be transitively included.

// IVector<T> flavor.
#define INHERITABLE_JSON_BACKED_VECTOR_SETTING(projectedType, type, name, jsonKey, ...)                                                   \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)                                                            \
public:                                                                                                                                   \
    /* Returns nullptr when no effective value exists.                                                                       */           \
    /* Otherwise returns a wrapper around a fresh shadow seeded from the effective JSON.                                     */           \
    /* Mutations on the wrapper auto-promote the setting into this layer's _json.                                            */           \
    type name()                                                                                                                           \
    {                                                                                                                                     \
        const auto val{ _get##name##Impl() };                                                                                             \
        if (!val || !*val)                                                                                                                \
        {                                                                                                                                 \
            return nullptr;                                                                                                               \
        }                                                                                                                                 \
        using ElementT = typename ::winrt::Microsoft::Terminal::Settings::Model::implementation::CollectionElementOf<type>::element_type; \
        auto strong = get_strong();                                                                                                       \
        return winrt::make<::winrt::Microsoft::Terminal::Settings::Model::implementation::JsonSyncVector<ElementT>>(                      \
            *val,                                                                                                                         \
            [strong](type const& current) {                                                                                               \
                auto temp = ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<type>{}.ToJson(current);                   \
                strong->_json[JsonKey(jsonKey)] = std::move(temp);                                                                        \
                strong->_logSettingSet(jsonKey);                                                                                          \
                /* TODO GH#12424: raise WriteSettings event */                                                                            \
            });                                                                                                                           \
    }                                                                                                                                     \
                                                                                                                                          \
    /* Whole-replace setter (for "create from scratch" call sites). */                                                                    \
    void name(const type& value)                                                                                                          \
    {                                                                                                                                     \
        const auto existingVal{ _get##name##FromThisLayer() };                                                                            \
        if (!existingVal.has_value() || existingVal.value() != value)                                                                     \
        {                                                                                                                                 \
            _logSettingSet(jsonKey);                                                                                                      \
        }                                                                                                                                 \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(                                                                \
            _json, jsonKey, value);                                                                                                       \
        /* TODO GH#12424: raise WriteSettings event */                                                                                    \
    }

// IMap<K, V> flavor.
#define INHERITABLE_JSON_BACKED_MAP_SETTING(projectedType, type, name, jsonKey, ...)                                            \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)                                                  \
public:                                                                                                                         \
    type name()                                                                                                                 \
    {                                                                                                                           \
        const auto val{ _get##name##Impl() };                                                                                   \
        if (!val || !*val)                                                                                                      \
        {                                                                                                                       \
            return nullptr;                                                                                                     \
        }                                                                                                                       \
        using KeyT = typename ::winrt::Microsoft::Terminal::Settings::Model::implementation::MapKeyValueOf<type>::key_type;     \
        using ValueT = typename ::winrt::Microsoft::Terminal::Settings::Model::implementation::MapKeyValueOf<type>::value_type; \
        auto strong = get_strong();                                                                                             \
        return winrt::make<::winrt::Microsoft::Terminal::Settings::Model::implementation::JsonSyncMap<KeyT, ValueT>>(           \
            *val,                                                                                                               \
            [strong](type const& current) {                                                                                     \
                auto temp = ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<type>{}.ToJson(current);         \
                strong->_json[JsonKey(jsonKey)] = std::move(temp);                                                              \
                strong->_logSettingSet(jsonKey);                                                                                \
                /* TODO GH#12424: raise WriteSettings event */                                                                  \
            });                                                                                                                 \
    }                                                                                                                           \
                                                                                                                                \
    void name(const type& value)                                                                                                \
    {                                                                                                                           \
        const auto existingVal{ _get##name##FromThisLayer() };                                                                  \
        if (!existingVal.has_value() || existingVal.value() != value)                                                           \
        {                                                                                                                       \
            _logSettingSet(jsonKey);                                                                                            \
        }                                                                                                                       \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(                                                      \
            _json, jsonKey, value);                                                                                             \
        /* TODO GH#12424: raise WriteSettings event */                                                                          \
    }

// =============================================================================
// Validation helper
// =============================================================================
// Expands to a statement that eagerly deserializes a single JSON-backed setting
// from this layer's _json, forcing its ConversionTrait conversion to run so a
// malformed value throws a JsonUtils::DeserializationError at parse time.
// Designed to be passed to an MTSM_*_SETTINGS(X) X-macro list inside a class's
// _ValidateThisLayer() override. Works for both regular and nullable JSON-backed
// settings, since both generate a _get<Name>FromThisLayer() helper.
#define MTSM_VALIDATE_SETTING(type, name, jsonKey, ...) \
    std::ignore = _get##name##FromThisLayer();
