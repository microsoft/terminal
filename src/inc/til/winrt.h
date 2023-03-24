// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        template<typename T>
        constexpr T winrt_empty_value() noexcept
        {
            if constexpr (std::is_base_of_v<winrt::Windows::Foundation::IUnknown, T>)
            {
                return nullptr;
            }
            else
            {
                return {};
            }
        }
    };

    template<typename T>
    struct property
    {
        property<T>(const T& defaultValue = details::winrt_empty_value<T>()) :
            _value{ defaultValue } {}

        const T& operator()() const
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
        operator bool() const requires std::is_same_v<T, winrt::hstring>
        {
            return !_value.empty();
        }
        operator bool() const requires std::convertible_to<T, bool>
        {
            return _value;
        }
        bool operator==(const property<T>& other) const
        {
            return _value == other._value;
        }
        bool operator!=(const property<T>& other) const
        {
            return _value != other._value;
        }
        bool operator==(const T& other) const
        {
            return _value == other;
        }
        bool operator!=(const T& other) const
        {
            return _value != other;
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
#ifdef WINRT_Windows_UI_Xaml_DataH

    using property_changed_event = event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler>;

    // This  unfortunately doesn't seem feasible. It's gonna just result in more
    // macros, which no one wants.
    //
    // Explanation can be found in the operator() below.
    /*
        template<typename T>
        struct observable_property
        {
            observable_property<T>(property_changed_event e) :
                _event{ e } {};
            observable_property<T>(property_changed_event e, T defaultValue) :
                _event{ e },
                _value{ defaultValue } {}

            T operator()()
            {
                return _value;
            }
            void operator()(const T& value)
            {
                if (_value != value)
                {
                    _value = value;

                    // This line right here is why we can't do this.
                    _event(*sender, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L## #name })
                    // 1. We don't know who the sender is, or would require `this` to
                    //    always be the first parameter to one of these
                    //    observable_property's.
                    //
                    // 2. We don't know what our own name is. We need to actually raise
                    //    an event with the name of the variable as the parameter. Only
                    //    way to do that is with something dumb like
                    //
                    // ```
                    // til::observable<int> Foo(this, L"Foo", 42)
                    // ```
                    //
                    // Which is just dang dumb
                }
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
            property_changed_event& _event;
        };

    #define OBSERVABLE(type, name, ...) til::observable_property<type> name{ this, L## #name, this.PropertyChanged, __VA_ARGS__ };
    */

#endif
}
