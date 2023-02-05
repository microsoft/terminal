// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<typename T>
    struct property
    {
        property<T>() = default;
        property<T>(T defaultValue) :
            _value{ defaultValue } {}

        T operator()()
        {
            return _value;
        }
        void operator()(const T& value)
        {
            _value = value;
        }
        void operator()(const T&& value)
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
        event<ArgsT>() = default;
        winrt::event_token operator()(const ArgsT& handler) { return _handlers.add(handler); }
        void operator()(const winrt::event_token& token) { _handlers.remove(token); }
        template<typename... Arg>
        void raise(Arg const&... args)
        {
            _handlers(args...);
        }
        winrt::event<ArgsT> _handlers;
    };

    template<typename ArgsT>
    struct forwarded_event
    {
        forwarded_event<ArgsT>() = delete;
        forwarded_event<ArgsT>(event<ArgsT>& other) :
            _origin{ other } {}
        forwarded_event<ArgsT>(forwarded_event<ArgsT>& other) :
            _origin{ other._origin } {}

        winrt::event_token operator()(const ArgsT& handler) { return _origin(handler); }
        void operator()(const winrt::event_token& token) { _origin(token); }
        template<typename... Arg>
        void raise(Arg const&... args)
        {
            _origin.raise(args...);
        }
        event<ArgsT>& _origin;
    };

    template<typename SenderT, typename ArgsT>
    struct typed_event
    {
        typed_event<SenderT, ArgsT>() = default;
        winrt::event_token operator()(const winrt::Windows::Foundation::TypedEventHandler<SenderT, ArgsT>& handler) { return _handlers.add(handler); }
        void operator()(const winrt::event_token& token) { _handlers.remove(token); }
        template<typename... Arg>
        void raise(Arg const&... args)
        {
            _handlers(args...);
        }
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<SenderT, ArgsT>> _handlers;
    };

    template<typename SenderT, typename ArgsT>
    struct forwarded_typed_event
    {
        forwarded_typed_event<SenderT, ArgsT>() = delete;
        forwarded_typed_event<SenderT, ArgsT>(typed_event<SenderT, ArgsT>& other) :
            _origin{ other } {}
        forwarded_typed_event<SenderT, ArgsT>(forwarded_typed_event<SenderT, ArgsT>& other) :
            _origin{ other._origin } {}

        winrt::event_token operator()(const winrt::Windows::Foundation::TypedEventHandler<SenderT, ArgsT>& handler) { return _origin(handler); }
        void operator()(const winrt::event_token& token) { _origin(token); }
        template<typename... Arg>
        void raise(Arg const&... args)
        {
            _origin.raise(args...);
        }
        typed_event<SenderT, ArgsT>& _origin;
    };
#endif

}
