// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#define TSM_NATIVE_LIFETIME_DEFINITIONS(type)                            \
    HRESULT __stdcall type::QueryInterface(REFIID iid, void** object) noexcept \
    { return base_type::QueryInterface(iid, object); }                   \
    ULONG __stdcall type::AddRef() noexcept { return base_type::AddRef(); } \
    ULONG __stdcall type::Release() noexcept { return base_type::Release(); } \
    winrt::com_ptr<type> type::get_strong() noexcept { return base_type::get_strong(); } \
    winrt::weak_ref<type> type::get_weak() { return base_type::get_weak(); }

#define TSM_NATIVE_PROPERTY_DEFINITIONS(owner, type, name, logChanges, jsonKey, ...) \
    bool owner::Has##name() const { return _##name.has_value(); }                 \
    void owner::Clear##name() { _##name = std::nullopt; }                         \
    type owner::name() const                                                     \
    {                                                                           \
        const auto value = _get##name##Impl();                                   \
        return value ? *value : type{ __VA_ARGS__ };                             \
    }                                                                           \
    void owner::name(const type& value)                                          \
    {                                                                           \
        if constexpr (logChanges)                                               \
        {                                                                       \
            if (!_##name || *_##name != value)                                  \
            {                                                                   \
                _logSettingSet(jsonKey);                                         \
            }                                                                   \
        }                                                                       \
        _##name = value;                                                        \
    }                                                                           \
    winrt::com_ptr<owner> owner::name##OverrideSource() const                    \
    {                                                                           \
        for (const auto& parent : _parents)                                     \
        {                                                                       \
            if (auto source = parent->_get##name##OverrideSourceImpl())         \
            {                                                                   \
                return source;                                                  \
            }                                                                   \
        }                                                                       \
        return nullptr;                                                         \
    }                                                                           \
    std::optional<type> owner::_get##name##Impl() const                          \
    {                                                                           \
        if (_##name)                                                            \
        {                                                                       \
            return _##name;                                                     \
        }                                                                       \
        for (const auto& parent : _parents)                                     \
        {                                                                       \
            if (auto value = parent->_get##name##Impl())                        \
            {                                                                   \
                return value;                                                   \
            }                                                                   \
        }                                                                       \
        return std::nullopt;                                                     \
    }                                                                           \
    winrt::com_ptr<owner> owner::_get##name##OverrideSourceImpl()                \
    {                                                                           \
        if (_##name)                                                            \
        {                                                                       \
            return get_strong();                                                \
        }                                                                       \
        for (const auto& parent : _parents)                                     \
        {                                                                       \
            if (auto source = parent->_get##name##OverrideSourceImpl())         \
            {                                                                   \
                return source;                                                  \
            }                                                                   \
        }                                                                       \
        return nullptr;                                                         \
    }                                                                           \
    std::pair<winrt::com_ptr<owner>, std::optional<type>> owner::_get##name##OverrideSourceAndValueImpl() \
    {                                                                           \
        if (auto source = _get##name##OverrideSourceImpl())                     \
        {                                                                       \
            return { source, source->_##name };                                 \
        }                                                                       \
        return { nullptr, std::nullopt };                                       \
    }
