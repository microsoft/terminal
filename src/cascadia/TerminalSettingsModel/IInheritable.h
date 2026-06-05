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

#include <memory>
#include <functional>

#include "JsonUtils.h"
#include "JsonSyncCollections.h"
#include "SettingsWriteNotifier.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    template<typename T>
    struct IInheritable : WriteNotifiable
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

        // The shared "request auto-save" sink (SetWriteSettingsSink,
        // _writeSettingsSink, NotifyWriteSettings) is inherited from
        // WriteNotifiable. The owning CascadiaSettings installs one shared sink on
        // every object in the *live* tree; mutators call NotifyWriteSettings().
        // The sink is intentionally NOT copied by Copy()/clone paths, so editor
        // clones (and the defaults fallback) never auto-save the user's settings.

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
}

// =============================================================================
// Inheritable setting macros
// =============================================================================
//
// Each macro generates Has<NAME>(), <NAME>OverrideSource(), the getter/setter,
// and Clear<NAME>(). The getter resolves this layer --> parent --> default.
//
// Storage strategies:
//   1. JSON-backed scalar/nullable (default). State lives in _json. The owning
//      class must declare `Json::Value _json{ Json::ValueType::objectValue };`.
//      Macros: INHERITABLE_SETTING, INHERITABLE_SETTING_WITH_LOGGING,
//              INHERITABLE_NULLABLE_SETTING.
//   2. JSON-backed collections (IVector<T> / IMap<K,V>). The getter returns a
//      JsonSyncVector / JsonSyncMap wrapper that mirrors in-place mutations into
//      _json. Macros: INHERITABLE_JSON_BACKED_VECTOR_SETTING,
//      INHERITABLE_JSON_BACKED_MAP_SETTING.
//   3. Backing-field. Only when the runtime needs state JSON can't carry — a
//      live sub-object (Profile::UnfocusedAppearance) or IMediaResource
//      resolution caches (Icon, shaders, BellSound, NewTabMenu). These use
//      _BASE_INHERITABLE_MUTABLE_SETTING (directly or via
//      INHERITABLE_MEDIA_RESOURCE_SETTING / _VECTOR_SETTING) and dual-write to
//      _json for serialization.
//
// Prefer (1)/(2). Reach for (3) only when state must persist outside _json.
// =============================================================================

// =============================================================================
// JSON-backed inheritable settings
// =============================================================================
// No std::optional<T> backing field — _json is the source of truth.
// Getters deserialize from _json via ConversionTrait on each call.
// Setters serialize to _json via SetValueForKey.
// Has/Clear check/modify _json directly.

// Shared core for JSON-backed inheritable settings (scalar and nullable).
// Provides Has, OverrideSource, Clear, and the private resolvers. The owning
// macro supplies the public getter/setter.
//
// Regular and nullable settings differ only in:
//   - storageType: the stored/returned value type (T, or IReference<T> for
//     nullable settings where a JSON null is a meaningful "no value"),
//   - hasExpr: presence at this layer. ConversionTrait<storageType> (JsonUtils)
//     supplies the conversion, so ConversionTrait<IReference<T>> gives the
//     nullable null/absent/value semantics for free.
#define _INHERITABLE_SETTING_CORE(projectedType, storageType, name, jsonKey, hasExpr) \
public:                                                                               \
    /* Returns true if the user explicitly set the value at this layer */             \
    bool Has##name() const                                                            \
    {                                                                                 \
        return hasExpr;                                                               \
    }                                                                                 \
                                                                                      \
    /* Returns the object that provides the resolved value */                         \
    projectedType name##OverrideSource()                                              \
    {                                                                                 \
        for (const auto& parent : _parents)                                           \
        {                                                                             \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })              \
            {                                                                         \
                return *source;                                                       \
            }                                                                         \
        }                                                                             \
        return nullptr;                                                               \
    }                                                                                 \
                                                                                      \
    /* Clear the user set value (revert to inherited/default) */                      \
    void Clear##name()                                                                \
    {                                                                                 \
        _json.removeMember(JsonKey(jsonKey));                                         \
        NotifyWriteSettings();                                         \
    }                                                                                 \
                                                                                      \
private:                                                                              \
    /* Read this layer's value only (no parent walk) */                               \
    std::optional<storageType> _get##name##FromThisLayer() const                      \
    {                                                                                 \
        if (Has##name())                                                              \
        {                                                                             \
            storageType val{};                                                        \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::GetValueForKey(        \
                _json, jsonKey, val);                                                 \
            return val;                                                               \
        }                                                                             \
        return std::nullopt;                                                          \
    }                                                                                 \
                                                                                      \
    /* Resolve the value: this layer --> parents --> nullopt */                       \
    std::optional<storageType> _get##name##Impl() const                               \
    {                                                                                 \
        if (auto val{ _get##name##FromThisLayer() })                                  \
        {                                                                             \
            return val;                                                               \
        }                                                                             \
        for (const auto& parent : _parents)                                           \
        {                                                                             \
            if (auto val{ parent->_get##name##Impl() })                               \
            {                                                                         \
                return val;                                                           \
            }                                                                         \
        }                                                                             \
        return std::nullopt;                                                          \
    }                                                                                 \
                                                                                      \
    /* Find the object that provides the resolved value */                            \
    auto _get##name##OverrideSourceImpl()->decltype(get_strong())                     \
    {                                                                                 \
        if (Has##name())                                                              \
        {                                                                             \
            return get_strong();                                                      \
        }                                                                             \
        for (const auto& parent : _parents)                                           \
        {                                                                             \
            if (auto source{ parent->_get##name##OverrideSourceImpl() })              \
            {                                                                         \
                return source;                                                        \
            }                                                                         \
        }                                                                             \
        return nullptr;                                                               \
    }

// Regular JSON-backed scalar base (presence = key present and non-null).
// Used directly when the owning class hand-writes its getter/setter (e.g. Guid).
#define _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, ...) \
    _INHERITABLE_SETTING_CORE(projectedType, type, name, jsonKey, (_json.isMember(jsonKey) && !_json[jsonKey].isNull()))

// Shared setter for JSON-backed settings that log changes (WITH_LOGGING and the
// JSON-backed collection macros). Logs jsonKey when the value actually changes.
#define _INHERITABLE_JSON_LOGGING_SETTER(type, name, jsonKey)              \
public:                                                                    \
    void name(const type& value)                                           \
    {                                                                      \
        const auto existingVal{ _get##name##FromThisLayer() };             \
        if (!existingVal.has_value() || existingVal.value() != value)      \
        {                                                                  \
            _logSettingSet(jsonKey);                                       \
        }                                                                  \
        ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey( \
            _json, jsonKey, value);                                        \
        NotifyWriteSettings();                     \
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
        NotifyWriteSettings();                                            \
    }

// JSON-backed inheritable setting with change logging.
// Same as INHERITABLE_SETTING, but the setter logs the change for telemetry.
#define INHERITABLE_SETTING_WITH_LOGGING(projectedType, type, name, jsonKey, ...) \
    _BASE_INHERITABLE_SETTING(projectedType, type, name, jsonKey, __VA_ARGS__)    \
public:                                                                           \
    /* Returns the resolved value: this layer --> inherited --> default */        \
    type name() const                                                             \
    {                                                                             \
        const auto val{ _get##name##Impl() };                                     \
        return val ? *val : type{ __VA_ARGS__ };                                  \
    }                                                                             \
    _INHERITABLE_JSON_LOGGING_SETTER(type, name, jsonKey)

// JSON-backed nullable inheritable setting.
// For settings where null is a valid explicit value (e.g., Foreground color).
//   Key absent  → inherit from parent
//   Key present + null  → explicitly "no value" (getter returns nullptr)
//   Key present + value → has a value
// Stored as IReference<type>; ConversionTrait<IReference<type>> handles the
// null/value mapping, so this reuses the same core as the regular setting.
#define INHERITABLE_NULLABLE_SETTING(projectedType, type, name, jsonKey, ...)                                                        \
    _INHERITABLE_SETTING_CORE(projectedType, winrt::Windows::Foundation::IReference<type>, name, jsonKey, (_json.isMember(jsonKey))) \
public:                                                                                                                              \
    /* Returns the resolved value: this layer --> inherited --> default */                                                           \
    winrt::Windows::Foundation::IReference<type> name() const                                                                        \
    {                                                                                                                                \
        const auto val{ _get##name##Impl() };                                                                                        \
        return val ? *val : winrt::Windows::Foundation::IReference<type>{ __VA_ARGS__ };                                             \
    }                                                                                                                                \
                                                                                                                                     \
    /* Write the value to _json (a null IReference is written as an explicit JSON null) */                                           \
    void name(const winrt::Windows::Foundation::IReference<type>& value)                                                             \
    {                                                                                                                                \
        if (value)                                                                                                                   \
        {                                                                                                                            \
            ::Microsoft::Terminal::Settings::Model::JsonUtils::SetValueForKey(_json, jsonKey, value.Value());                        \
        }                                                                                                                            \
        else                                                                                                                         \
        {                                                                                                                            \
            /* explicitly set to null (not the same as clearing) */                                                                  \
            _json[JsonKey(jsonKey)] = Json::nullValue;                                                                               \
        }                                                                                                                            \
        NotifyWriteSettings();                                                       \
    }

// =============================================================================
// Mutable backing-field inheritable settings
// =============================================================================
// State lives in a std::optional<T> backing field instead of _json. Use only
// when runtime state must persist across getter calls and JSON can't carry it.
// Media settings dual-write to _json for serialization.

// Shared base for backing-field settings: Has, OverrideSource, the backing
// field, and the parent-walk resolvers. Each owning macro/class supplies its own
// getter/setter and Clear<Name>() (backing-only, or dual-write for media).
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

// IMediaResource backing-field setting (Icon, shaders). The backing field holds
// the runtime-resolved object; setter and Clear dual-write to _json so auto-save
// stays consistent. Args: projectedType, name, jsonKey, defaultValue.
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
        NotifyWriteSettings();                                                                            \
    }                                                                                                                             \
                                                                                                                                  \
    /* Dual-clear: backing field + _json key. Both must clear so auto-save doesn't */                                             \
    /* re-persist the cleared value. */                                                                                           \
    void Clear##name()                                                                                                            \
    {                                                                                                                             \
        _##name = std::nullopt;                                                                                                   \
        _json.removeMember(JsonKey(jsonKey));                                                                                     \
        _logSettingSet(jsonKey);                                                                                                  \
        NotifyWriteSettings();                                                                            \
    }

// IMediaResource vector setting (BellSound): needs both IMediaResource
// resolution and in-place IVector mutation. The backing field holds the resolved
// instances; setter, Clear, and wrapper mutations dual-write to _json.
//
// Asymmetric getter: when this layer Has<Name>(), returns a JsonSyncVector
// wrapper over the backing field (mutations dual-write _##name + _json). When
// inherited, returns the parent's IVector directly — callers must materialize a
// local copy via the whole-replace setter before mutating (see
// ProfileViewModel::_PrepareModelForBellSoundModification). This preserves parent
// isolation. Args: projectedType, name, jsonKey, fallback (e.g. nullptr).
// Requires JsonSyncCollections.h.
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
                    strong->NotifyWriteSettings();                                                             \
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
        NotifyWriteSettings();                                                                         \
    }                                                                                                                          \
                                                                                                                               \
    /* Dual-clear: backing field + _json key. */                                                                               \
    void Clear##name()                                                                                                         \
    {                                                                                                                          \
        _##name = std::nullopt;                                                                                                \
        _json.removeMember(JsonKey(jsonKey));                                                                                  \
        _logSettingSet(jsonKey);                                                                                               \
        NotifyWriteSettings();                                                                         \
    }

// JSON-backed collection settings (IVector<T> / IMap<K,V>) that callers mutate
// in place. The getter returns a JsonSyncVector / JsonSyncMap wrapper that
// mirrors mutations into _json[jsonKey], or nullptr when no effective value
// exists. The wrapper's shadow is independent of parents, so children never
// mutate a parent. Requires JsonSyncCollections.h.

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
                strong->NotifyWriteSettings();                                                                            \
            });                                                                                                                           \
    }                                                                                                                                     \
                                                                                                                                          \
    /* Whole-replace setter (for "create from scratch" call sites). */                                                                    \
    _INHERITABLE_JSON_LOGGING_SETTER(type, name, jsonKey)

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
                strong->NotifyWriteSettings();                                                                  \
            });                                                                                                                 \
    }                                                                                                                           \
                                                                                                                                \
    _INHERITABLE_JSON_LOGGING_SETTER(type, name, jsonKey)

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
