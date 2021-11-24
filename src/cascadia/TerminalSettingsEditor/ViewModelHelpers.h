// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/cppwinrt_utils.h"

template<typename T>
struct ViewModelHelper
{
public:
    winrt::event_token PropertyChanged(::winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return _propertyChangedHandlers.add(handler);
    }

    void PropertyChanged(winrt::event_token const& token)
    {
        _propertyChangedHandlers.remove(token);
    }

protected:
    void _NotifyChangeCore(const std::wstring_view name)
    {
        _propertyChangedHandlers(*static_cast<T*>(this), ::winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs{ name });
    }

    // template recursion base case: single dispatch
    void _NotifyChanges(const std::wstring_view name) { _NotifyChangeCore(name); }

    template<typename... Args>
    void _NotifyChanges(const std::wstring_view name, Args&&... more)
    {
        _NotifyChangeCore(name);
        _NotifyChanges(std::forward<Args>(more)...);
    }

private:
    winrt::event<::winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> _propertyChangedHandlers;
};

#define _GET_SETTING_FUNC(target, name) \
public:                                 \
    auto name() const noexcept { return target.name(); };

#define _SET_SETTING_FUNC(target, name)              \
public:                                              \
    template<typename T>                             \
    void name(const T& value)                        \
    {                                                \
        if (name() != value)                         \
        {                                            \
            target.name(value);                      \
            _NotifyChanges(L"Has" #name, L## #name); \
        }                                            \
    }

#define _HAS_SETTING_FUNC(target, name) \
public:                                 \
    bool Has##name() { return target.Has##name(); }

#define _CLEAR_SETTING_FUNC(target, name)            \
public:                                              \
    void Clear##name()                               \
    {                                                \
        const auto hadValue{ target.Has##name() };   \
        target.Clear##name();                        \
        if (hadValue)                                \
        {                                            \
            _NotifyChanges(L"Has" #name, L## #name); \
        }                                            \
    }

#define _GET_OVERRIDE_SOURCE_SETTING_FUNC(target, name) \
public:                                                 \
    auto name##OverrideSource() { return target.name##OverrideSource(); }

#define _BASE_OBSERVABLE_PROJECTED_SETTING(target, name) \
    _GET_SETTING_FUNC(target, name)                      \
    _SET_SETTING_FUNC(target, name)                      \
    _HAS_SETTING_FUNC(target, name)

// Defines a setting that reflects another object's same-named
// setting.
#define OBSERVABLE_PROJECTED_SETTING(target, name)   \
    _BASE_OBSERVABLE_PROJECTED_SETTING(target, name) \
    _CLEAR_SETTING_FUNC(target, name)                \
    _GET_OVERRIDE_SOURCE_SETTING_FUNC(target, name)

// Defines a setting that reflects another object's same-named
// setting, but which cannot be erased.
#define PERMANENT_OBSERVABLE_PROJECTED_SETTING(target, name) \
    _BASE_OBSERVABLE_PROJECTED_SETTING(target, name)

// Defines a basic observable property that uses the _NotifyChanges
// system from ViewModelHelper. This is very similar to WINRT_OBSERVABLE_PROPERTY
// except it leverages _NotifyChanges.
#define VIEW_MODEL_OBSERVABLE_PROPERTY(type, name, ...) \
public:                                                 \
    type name() const noexcept { return _##name; };     \
    void name(const type& value)                        \
    {                                                   \
        if (_##name != value)                           \
        {                                               \
            _##name = value;                            \
            _NotifyChanges(L## #name);                  \
        }                                               \
    };                                                  \
                                                        \
private:                                                \
    type _##name{ __VA_ARGS__ };
