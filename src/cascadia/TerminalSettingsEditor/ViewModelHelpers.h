// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

template<typename T>
struct ViewModelHelper
{
public:
    winrt::event_token PropertyChanged(const ::winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler& handler)
    {
        return _propertyChangedHandlers.add(handler);
    }

    void PropertyChanged(const winrt::event_token& token)
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

protected:
    winrt::event<::winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> _propertyChangedHandlers;
};

#define GETSET_OBSERVABLE_PROJECTED_SETTING(target, name) \
public:                                                   \
    auto name() const                                     \
    {                                                     \
        return target.name();                             \
    };                                                    \
    template<typename T>                                  \
    void name(const T& value)                             \
    {                                                     \
        if (target.name() != value)                       \
        {                                                 \
            target.name(value);                           \
            _NotifyChanges(L"Has" #name, L## #name);      \
        }                                                 \
    }

#define _BASE_OBSERVABLE_PROJECTED_SETTING(target, name) \
    GETSET_OBSERVABLE_PROJECTED_SETTING(target, name)    \
    bool Has##name() const                               \
    {                                                    \
        return target.Has##name();                       \
    }

// Defines a setting that reflects another object's same-named
// setting.
#define OBSERVABLE_PROJECTED_SETTING(target, name)   \
    _BASE_OBSERVABLE_PROJECTED_SETTING(target, name) \
    void Clear##name()                               \
    {                                                \
        const auto t = target;                       \
        const auto hadValue{ t.Has##name() };        \
        t.Clear##name();                             \
        if (hadValue)                                \
        {                                            \
            _NotifyChanges(L"Has" #name, L## #name); \
        }                                            \
    }                                                \
    auto name##OverrideSource() const                \
    {                                                \
        return target.name##OverrideSource();        \
    }

// Defines a setting that reflects another object's same-named
// setting, but which cannot be erased.
#define PERMANENT_OBSERVABLE_PROJECTED_SETTING(target, name) \
    _BASE_OBSERVABLE_PROJECTED_SETTING(target, name)

// Defines a basic observable property that uses the _NotifyChanges
// system from ViewModelHelper. This is very similar to WINRT_OBSERVABLE_PROPERTY
// except it leverages _NotifyChanges.
#define VIEW_MODEL_OBSERVABLE_PROPERTY(type, name, ...) \
public:                                                 \
    type name() const noexcept                          \
    {                                                   \
        return _##name;                                 \
    };                                                  \
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
