// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>



winrt::hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);
winrt::hstring KeyToString(winrt::Windows::System::VirtualKey key);

#define DEFINE_PROPERTYCHANGED()                                                                            \
public:                                                                                                     \
    event_token PropertyChanged(winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler) \
    {                                                                                                       \
        return _PropertyChanged.add(handler);                                                               \
    }                                                                                                       \
                                                                                                            \
    void PropertyChanged(winrt::event_token const& token)                                                   \
    {                                                                                                       \
        _PropertyChanged.remove(token);                                                                     \
    }                                                                                                       \
                                                                                                            \
private:                                                                                                    \
    winrt::event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> _PropertyChanged;

// TODO GH#1564: Implement Settings UI
// The macros below were copied from cppwinrt_utils.h
// They can be removed once the Settings UI is integrated

// This is a helper macro for both declaring the signature of a callback (nee event) and
// defining the body. Winrt callbacks need a method for adding a delegate to the
// callback and removing the delegate. This macro will both declare the method
// signatures and define them both for you, because they don't really vary from
// event to event.
// Use this in a class's header if you have a "delegate" type in your IDL.
#define WINRT_CALLBACK(name, args)                                                          \
public:                                                                                     \
    winrt::event_token name(args const& handler) { return _##name##Handlers.add(handler); } \
    void name(winrt::event_token const& token) { _##name##Handlers.remove(token); }         \
                                                                                            \
private:                                                                                    \
    winrt::event<args> _##name##Handlers;

// Use this macro to quickly implement both the getter and setter for an
// observable property. This is similar to the GETSET_PROPERTY macro above,
// except this will also raise a PropertyChanged event with the name of the
// property that has changed inside of the setter. This also implements a
// private _setName() method, that the class can internally use to change the
// value when it _knows_ it doesn't need to raise the PropertyChanged event
// (like when the class is being initialized).
#define OBSERVABLE_GETSET_PROPERTY(type, name, event, ...)                             \
public:                                                                                \
    type name() { return _##name; };                                                   \
    void name(const type& value)                                                       \
    {                                                                                  \
        if (_##name != value)                                                          \
        {                                                                              \
            const_cast<type&>(_##name) = value;                                        \
            event(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L#name }); \
        }                                                                              \
    };                                                                                 \
                                                                                       \
private:                                                                               \
    const type _##name{ __VA_ARGS__ };                                                 \
    void _set##name(const type& value)                                                 \
    {                                                                                  \
        const_cast<type&>(_##name) = value;                                            \
    };

// Use this macro for quickly defining the factory_implementation part of a
// class. CppWinrt requires these for the compiler, but more often than not,
// they require no customization. See
// https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/implements#marker-types
// and https://docs.microsoft.com/en-us/uwp/cpp-ref-for-winrt/static-lifetime
// for examples of when you might _not_ want to use this.
#define BASIC_FACTORY(typeName)                                       \
    struct typeName : typeName##T<typeName, implementation::typeName> \
    {                                                                 \
    };
