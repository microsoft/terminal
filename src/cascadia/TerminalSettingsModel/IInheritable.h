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
// There are two storage strategies:
//
//   1. JSON-backed (preferred): All state lives in _json (a Json::Value member).
//      Use these for any setting with a ConversionTrait. The owning class must
//      declare a `Json::Value _json{ Json::ValueType::objectValue };` member.
//
//   2. Mutable backing-field (exception): State lives in a std::optional<T> member.
//      Use these only for types that callers mutate in place after retrieval, such
//      as IMediaResource (resolved via ResolveMediaResources), IVector, and IMap.
//      A JSON-backed getter would deserialize a fresh copy on each call, losing
//      those in-place mutations.
//
// When adding a new setting, use the JSON-backed macros unless the type requires
// in-place mutation. Existing backing-field settings should be migrated to
// JSON-backed as their mutation patterns are resolved.
//
// Available macros (JSON-backed):
//   INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)
//   INHERITABLE_SETTING_WITH_LOGGING(projectedType, type, name, jsonKey, ...)
//   INHERITABLE_NULLABLE_SETTING(projectedType, type, name, jsonKey, ...)
//   _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)
//
// Available macros (mutable backing-field):
//   INHERITABLE_MUTABLE_SETTING(projectedType, type, name, ...)
//   INHERITABLE_NULLABLE_MUTABLE_SETTING(projectedType, type, name, ...)
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
#define _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)              \
public:                                                                                  \
    /* Returns true if the user explicitly set the value, false otherwise */             \
    bool Has##name() const                                                               \
    {                                                                                    \
        return _json.isMember(jsonKey) && !_json[jsonKey].isNull();                      \
    }                                                                                    \
                                                                                         \
    /* Returns the object that provides the resolved value */                            \
    projectedType name##OverrideSource()                                                 \
    {                                                                                    \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return *source;                                                          \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no source was found*/                                                          \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Clear the user set value */                                                       \
    void Clear##name()                                                                   \
    {                                                                                    \
        _json.removeMember(JsonKey(jsonKey));                                             \
    }                                                                                    \
                                                                                         \
private:                                                                                 \
    /* Read value from this layer's _json only (no parent walk) */                       \
    std::optional<type> _get##name##FromThisLayer() const                                \
    {                                                                                    \
        if (Has##name())                                                                 \
        {                                                                                \
            type val{ __VA_ARGS__ };                                                     \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey(            \
                _json, jsonKey, val);                                                     \
            return val;                                                                  \
        }                                                                                \
        return std::nullopt;                                                             \
    }                                                                                    \
                                                                                         \
    /* Resolve the value: this layer --> parents --> nullopt */                           \
    std::optional<type> _get##name##Impl() const                                         \
    {                                                                                    \
        /*return value from this layer*/                                                 \
        if (auto val{ _get##name##FromThisLayer() })                                     \
        {                                                                                \
            return val;                                                                  \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find a value*/                                      \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto val{ parent->_get##name##Impl() })                                  \
            {                                                                            \
                return val;                                                              \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return std::nullopt;                                                             \
    }                                                                                    \
                                                                                         \
    /* Find the object that provides the resolved value */                               \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())                        \
    {                                                                                    \
        /*we have a value*/                                                              \
        if (Has##name())                                                                 \
        {                                                                                \
            return get_strong();                                                         \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return source;                                                           \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Find the override source and the value it provides */                             \
    auto _get##name##OverrideSourceAndValueImpl()                                        \
        ->std::pair<decltype(get_strong()), std::optional<type>>                         \
    {                                                                                    \
        /*we have a value*/                                                              \
        if (Has##name())                                                                 \
        {                                                                                \
            return { get_strong(), _get##name##FromThisLayer() };                        \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return { source, source->_get##name##FromThisLayer() };                  \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return { nullptr, std::nullopt };                                                \
    }

// JSON-backed inheritable setting.
// Getter returns the resolved value: this layer --> inherited --> default.
// Setter writes the value to _json.
#define INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...)                     \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)           \
public:                                                                                  \
    /* Returns the resolved value for this setting */                                    \
    /* fallback: this layer --> inherited value --> default */                            \
    type name() const                                                                    \
    {                                                                                    \
        const auto val{ _get##name##Impl() };                                            \
        return val ? *val : type{ __VA_ARGS__ };                                         \
    }                                                                                    \
                                                                                         \
    /* Overwrite the user set value in _json */                                          \
    void name(const type& value)                                                         \
    {                                                                                    \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(                \
            _json, jsonKey, value);                                                       \
    }

// JSON-backed inheritable setting with change logging.
// Same as INHERITABLE_SETTING, but the setter also calls _logSettingSet(jsonKey)
// when the value actually changes (for settings change telemetry).
#define INHERITABLE_SETTING_WITH_LOGGING(projectedType, type, name, jsonKey, ...)        \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)           \
public:                                                                                  \
    /* Returns the resolved value for this setting */                                    \
    /* fallback: this layer --> inherited value --> default */                            \
    type name() const                                                                    \
    {                                                                                    \
        const auto val{ _get##name##Impl() };                                            \
        return val ? *val : type{ __VA_ARGS__ };                                         \
    }                                                                                    \
                                                                                         \
    /* Overwrite the user set value, log the change, and write to _json */               \
    void name(const type& value)                                                         \
    {                                                                                    \
        const auto existingVal{ _get##name##FromThisLayer() };                           \
        if (!existingVal.has_value() || existingVal.value() != value)                    \
        {                                                                                \
            _logSettingSet(jsonKey);                                                      \
        }                                                                                \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(                \
            _json, jsonKey, value);                                                       \
    }

// JSON-backed nullable inheritable setting.
// For settings where null is a valid explicit value (e.g., Foreground color).
//   Key absent  → inherit from parent
//   Key present + null  → explicitly "no value" (getter returns nullptr)
//   Key present + value → has a value
// Getter returns IReference<T>.
#define INHERITABLE_NULLABLE_SETTING(projectedType, type, name, jsonKey, ...)             \
public:                                                                                  \
    /* Key presence means explicitly set (even if value is null) */                      \
    bool Has##name() const                                                               \
    {                                                                                    \
        return _json.isMember(jsonKey);                                                  \
    }                                                                                    \
                                                                                         \
    /* Returns the object that provides the resolved value */                            \
    projectedType name##OverrideSource()                                                 \
    {                                                                                    \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return *source;                                                          \
            }                                                                            \
        }                                                                                \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Clear the user set value (remove key entirely → inherit from parent) */           \
    void Clear##name()                                                                   \
    {                                                                                    \
        _json.removeMember(JsonKey(jsonKey));                                             \
    }                                                                                    \
                                                                                         \
    /* Returns the resolved value for this setting */                                    \
    /* fallback: this layer --> inherited value --> default */                            \
    winrt::Windows::Foundation::IReference<type> name() const                            \
    {                                                                                    \
        const auto val{ _get##name##Impl() };                                            \
        if (val)                                                                         \
        {                                                                                \
            if (*val)                                                                    \
            {                                                                            \
                return **val;                                                            \
            }                                                                            \
            return nullptr;                                                              \
        }                                                                                \
        return winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ };              \
    }                                                                                    \
                                                                                         \
    /* Overwrite the user set value in _json */                                          \
    void name(const winrt::Windows::Foundation::IReference<type>& value)                 \
    {                                                                                    \
        if (value)                                                                       \
        {                                                                                \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(            \
                _json, jsonKey, value.Value());                                           \
        }                                                                                \
        else                                                                             \
        {                                                                                \
            /* explicitly set to null (not the same as clearing) */                      \
            _json[JsonKey(jsonKey)] = Json::nullValue;                                   \
        }                                                                                \
    }                                                                                    \
                                                                                         \
private:                                                                                 \
    /* Read nullable value from this layer's _json only (no parent walk) */              \
    /* Returns: nullopt = not set. optional{nullopt} = explicitly null. */               \
    /*          optional{optional{val}} = has value. */                                  \
    NullableSetting<type> _get##name##FromThisLayer() const                              \
    {                                                                                    \
        if (_json.isMember(jsonKey))                                                     \
        {                                                                                \
            if (_json[jsonKey].isNull())                                                  \
            {                                                                            \
                return std::optional<type>{ std::nullopt };                               \
            }                                                                            \
            type val{};                                                                  \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey(            \
                _json, jsonKey, val);                                                     \
            return std::optional<type>{ val };                                            \
        }                                                                                \
        return std::nullopt;                                                             \
    }                                                                                    \
                                                                                         \
    /* Resolve the nullable value: this layer --> parents --> nullopt */                  \
    NullableSetting<type> _get##name##Impl() const                                       \
    {                                                                                    \
        if (auto val{ _get##name##FromThisLayer() })                                     \
        {                                                                                \
            return val;                                                                  \
        }                                                                                \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto val{ parent->_get##name##Impl() })                                  \
            {                                                                            \
                return val;                                                              \
            }                                                                            \
        }                                                                                \
        return std::nullopt;                                                             \
    }                                                                                    \
                                                                                         \
    /* Find the object that provides the resolved value */                               \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())                        \
    {                                                                                    \
        if (Has##name())                                                                 \
        {                                                                                \
            return get_strong();                                                         \
        }                                                                                \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return source;                                                           \
            }                                                                            \
        }                                                                                \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Find the override source and the value it provides */                             \
    auto _get##name##OverrideSourceAndValueImpl()                                        \
        ->std::pair<decltype(get_strong()), NullableSetting<type>>                        \
    {                                                                                    \
        if (Has##name())                                                                 \
        {                                                                                \
            return { get_strong(), _get##name##FromThisLayer() };                        \
        }                                                                                \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return { source, source->_get##name##FromThisLayer() };                  \
            }                                                                            \
        }                                                                                \
        return { nullptr, std::nullopt };                                                \
    }

// =============================================================================
// Mutable backing-field inheritable settings
// =============================================================================
// These use a std::optional<T> backing field instead of _json.
//
// Use these ONLY for types that callers mutate in place after retrieval.
// A JSON-backed getter would deserialize a fresh copy on each call, so
// in-place mutations (e.g. ResolveMediaResources) would be lost.
//
// Current uses:
//   Profile:           Icon, EnvironmentVariables, BellSound, UnfocusedAppearance
//   GlobalAppSettings: DisabledProfileSources, NewTabMenu
//   AppearanceConfig:  PixelShaderPath, PixelShaderImagePath, BackgroundImagePath,
//                      DarkColorSchemeName, LightColorSchemeName
//   FontConfig:        FontAxes, FontFeatures

// Shared base for backing-field inheritable settings.
// Provides Has, Clear, OverrideSource, and the private implementation helpers.
#define _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, storageType, name, ...)         \
public:                                                                                  \
    /* Returns true if the user explicitly set the value, false otherwise */             \
    bool Has##name() const                                                               \
    {                                                                                    \
        return _##name.has_value();                                                      \
    }                                                                                    \
                                                                                         \
    /* Returns the object that provides the resolved value */                            \
    projectedType name##OverrideSource()                                                 \
    {                                                                                    \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return *source;                                                          \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no source was found*/                                                          \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Clear the user set value */                                                       \
    void Clear##name()                                                                   \
    {                                                                                    \
        _##name = std::nullopt;                                                          \
    }                                                                                    \
                                                                                         \
private:                                                                                 \
    storageType _##name{ std::nullopt };                                                 \
                                                                                         \
    /* Resolve the value: this layer --> parents --> nullopt */                           \
    storageType _get##name##Impl() const                                                 \
    {                                                                                    \
        /*return user set value*/                                                        \
        if (_##name)                                                                     \
        {                                                                                \
            return _##name;                                                              \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find a value*/                                      \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto val{ parent->_get##name##Impl() })                                  \
            {                                                                            \
                return val;                                                              \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return std::nullopt;                                                             \
    }                                                                                    \
                                                                                         \
    /* Find the object that provides the resolved value */                               \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())                        \
    {                                                                                    \
        /*we have a value*/                                                              \
        if (_##name)                                                                     \
        {                                                                                \
            return get_strong();                                                         \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return source;                                                           \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return nullptr;                                                                  \
    }                                                                                    \
                                                                                         \
    /* Find the override source and the value it provides */                             \
    auto _get##name##OverrideSourceAndValueImpl()                                        \
        ->std::pair<decltype(get_strong()), storageType>                                 \
    {                                                                                    \
        /*we have a value*/                                                              \
        if (_##name)                                                                     \
        {                                                                                \
            return { get_strong(), _##name };                                            \
        }                                                                                \
                                                                                         \
        /*iterate through parents to find one with a value*/                             \
        for (const auto& parent : _parents)                                              \
        {                                                                                \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })                 \
            {                                                                            \
                return { source, source->_##name };                                      \
            }                                                                            \
        }                                                                                \
                                                                                         \
        /*no value was found*/                                                           \
        return { nullptr, std::nullopt };                                                \
    }

// Mutable backing-field setting.
// Getter returns the resolved value: this layer --> inherited --> default.
// Setter writes to the std::optional<T> backing field.
#define INHERITABLE_MUTABLE_SETTING(projectedType, type, name, ...)                      \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, std::optional<type>, name, ...)     \
public:                                                                                  \
    /* Returns the resolved value for this setting */                                    \
    /* fallback: this layer --> inherited value --> default */                            \
    type name() const                                                                    \
    {                                                                                    \
        const auto val{ _get##name##Impl() };                                            \
        return val ? *val : type{ __VA_ARGS__ };                                         \
    }                                                                                    \
                                                                                         \
    /* Overwrite the user set value */                                                   \
    void name(const type& value)                                                         \
    {                                                                                    \
        _##name = value;                                                                 \
    }

// Mutable nullable backing-field setting.
// Same as INHERITABLE_MUTABLE_SETTING, but for optional settings where null is
// a valid explicit value. Getter returns IReference<T>.
#define INHERITABLE_NULLABLE_MUTABLE_SETTING(projectedType, type, name, ...)              \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, NullableSetting<type>, name, ...)   \
public:                                                                                  \
    /* Returns the resolved value for this setting */                                    \
    /* fallback: this layer --> inherited value --> default */                            \
    winrt::Windows::Foundation::IReference<type> name() const                            \
    {                                                                                    \
        const auto val{ _get##name##Impl() };                                            \
        if (val)                                                                         \
        {                                                                                \
            if (*val)                                                                    \
            {                                                                            \
                return **val;                                                            \
            }                                                                            \
            return nullptr;                                                              \
        }                                                                                \
        return winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ };              \
    }                                                                                    \
                                                                                         \
    /* Overwrite the user set value */                                                   \
    void name(const winrt::Windows::Foundation::IReference<type>& value)                 \
    {                                                                                    \
        if (value)                                                                       \
        {                                                                                \
            _##name = std::optional<type>{ value.Value() };                              \
        }                                                                                \
        else                                                                             \
        {                                                                                \
            /* explicitly set to null (not the same as clearing) */                      \
            _##name = std::optional<type>{ std::nullopt };                               \
        }                                                                                \
    }

// =============================================================================
// IMediaResource settings with backing fields for resolution lifecycle.
// =============================================================================
//
// Use for settings of type IMediaResource that need in-place resolution
// (e.g., Icon, PixelShaderPath, BackgroundImagePath). The backing field
// holds the runtime-resolved object. _json is the source of truth for
// serialization. The setter dual-writes to both.
//
// Parameters:
//   projectedType - the WinRT projected type (e.g., Model::Profile)
//   name          - the setting name (e.g., Icon)
//   jsonKey       - the JSON key (e.g., "icon")
//   ...           - default value (e.g., implementation::MediaResource::Empty())
//
#define INHERITABLE_MEDIA_RESOURCE_SETTING(projectedType, name, jsonKey, ...)                       \
    _BASE_INHERITABLE_MUTABLE_SETTING(projectedType, std::optional<IMediaResource>, name,          \
                                      implementation::MediaResource::Empty())                       \
public:                                                                                            \
    /* Returns the resolved value: this layer --> inherited --> default */                          \
    IMediaResource name() const                                                                    \
    {                                                                                              \
        const auto v{ _get##name##Impl() };                                                        \
        return v ? *v : IMediaResource{ __VA_ARGS__ };                                             \
    }                                                                                              \
                                                                                                   \
    /* Dual-write: backing field (for resolution) + _json (for serialization) */                   \
    void name(const IMediaResource& value)                                                         \
    {                                                                                              \
        _##name = value;                                                                           \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(_json, jsonKey, value);   \
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
