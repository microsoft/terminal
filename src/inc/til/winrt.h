// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<typename T>
    struct property
    {
        property<T>(){};
        property<T>(T defaultValue) :
            _value{ defaultValue } {}
        T& operator()()
        {
            return _value;
        }
        void operator()(const T& value)
        {
            _value = value;
        }
        property<T>& operator=(const property<T>& other)
        {
            _value = other._value;
            return *this;
        }
        property<T>& operator=(const T& newValue)
        {
            _value = newValue;
            return *this;
        }

    private:
        T _value;
    };

#ifdef WINRT_Windows_Foundation_H

    template<typename ArgsT>
    struct event
    {
        winrt::event_token operator()(const ArgsT& handler) { return _handlers.add(handler); }
        void operator()(const winrt::event_token& token) { _handlers.remove(token); }
        template<typename... Arg>
        void raise(Arg const&... args)
        {
            _handlers(args...);
        }
        winrt::event<ArgsT> _handlers;
    };

#endif

}
