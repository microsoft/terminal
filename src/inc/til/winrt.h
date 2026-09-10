// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<typename T>
    struct property
    {
        explicit constexpr property(auto&&... args) noexcept(std::is_nothrow_constructible_v<T, decltype(args)...>) :
            _value{ std::forward<decltype(args)>(args)... } {}

        property& operator=(const property& other) = default;

        T operator()() const noexcept(std::is_nothrow_copy_constructible<T>::value)
        {
            return _value;
        }
        void operator()(auto&& arg)
        {
            _value = std::forward<decltype(arg)>(arg);
        }
        explicit operator bool() const noexcept
        {
#ifdef WINRT_Windows_Foundation_H
            if constexpr (std::is_same_v<T, winrt::hstring>)
            {
                return !_value.empty();
            }
            else
#endif
            {
                return _value;
            }
        }
        bool operator==(const property& other) const noexcept
        {
            return _value == other._value;
        }
        bool operator!=(const property& other) const noexcept
        {
            return _value != other._value;
        }
        bool operator==(const T& other) const noexcept
        {
            return _value == other;
        }
        bool operator!=(const T& other) const noexcept
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
        explicit operator bool() const noexcept { return static_cast<bool>(_handlers); }
        winrt::event_token operator()(const ArgsT& handler) { return _handlers.add(handler); }
        void operator()(winrt::event_token token) { _handlers.remove(token); }

        void raise(auto&&... args)
        {
            _handlers(std::forward<decltype(args)>(args)...);
        }

    private:
        winrt::event<ArgsT> _handlers;
    };

    template<typename SenderT = winrt::Windows::Foundation::IInspectable, typename ArgsT = winrt::Windows::Foundation::IInspectable>
    using typed_event = til::event<winrt::Windows::Foundation::TypedEventHandler<SenderT, ArgsT>>;

    // Creates an auto-revoked event registration backed by a weak reference to
    // its handler owner. The event member pointer selects the generated
    // two-argument auto_revoke overload and allows its delegate type to be
    // deduced without naming it at the call site.
    template<typename Source, typename EventBase, typename Revoker, typename Delegate, typename Owner, typename Handler>
        requires std::is_member_function_pointer_v<Handler>
    [[nodiscard]] Revoker event_handler(const Source& source,
                                        Revoker (EventBase::*event)(winrt::auto_revoke_t, const Delegate&) const,
                                        winrt::weak_ref<Owner> owner,
                                        Handler handler)
    {
        return (source.*event)(winrt::auto_revoke, Delegate{ std::move(owner), std::move(handler) });
    }

    template<typename Source, typename EventBase, typename Revoker, typename Delegate, typename Owner, typename Handler>
        requires std::is_member_function_pointer_v<Handler>
    [[nodiscard]] Revoker event_handler(const Source& source,
                                        Revoker (EventBase::*event)(winrt::auto_revoke_t, const Delegate&) const,
                                        Owner* owner,
                                        Handler handler)
    {
        return event_handler(source, event, owner->get_weak(), std::move(handler));
    }

    // C++/WinRT generates a distinct revoker type for every event, which makes
    // storing them unnecessarily verbose. This move-only type erases those
    // types while preserving their normal RAII behavior.
    class event_revoker
    {
        static constexpr size_t _storageSize = 2 * sizeof(uint64_t);

        template<typename T>
        static constexpr bool _is_supported_revoker =
            sizeof(T) <= _storageSize &&
            alignof(T) <= alignof(uint64_t) &&
            std::is_nothrow_move_constructible_v<T> &&
            std::is_nothrow_destructible_v<T> &&
            requires(T& value, const T& constValue) {
                { value.revoke() } noexcept -> std::same_as<void>;
                { static_cast<bool>(constValue) } noexcept -> std::same_as<bool>;
            };

    public:
        event_revoker() noexcept = default;

        template<typename T>
            requires(!std::is_same_v<std::remove_cvref_t<T>, event_revoker> &&
                     _is_supported_revoker<std::remove_cvref_t<T>> &&
                     std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
        event_revoker(T&& revoker) noexcept
        {
            _emplace(std::forward<T>(revoker));
        }

        event_revoker(event_revoker&& other) noexcept
        {
            _move_from(std::move(other));
        }

        event_revoker& operator=(event_revoker&& other) noexcept
        {
            if (this != &other)
            {
                event_revoker{ std::move(other) }.swap(*this);
            }
            return *this;
        }

        template<typename T>
            requires(!std::is_same_v<std::remove_cvref_t<T>, event_revoker> &&
                     _is_supported_revoker<std::remove_cvref_t<T>> &&
                     std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T>)
        event_revoker& operator=(T&& revoker) noexcept
        {
            event_revoker{ std::forward<T>(revoker) }.swap(*this);
            return *this;
        }

        event_revoker(const event_revoker&) = delete;
        event_revoker& operator=(const event_revoker&) = delete;

        ~event_revoker() noexcept
        {
            revoke();
        }

        void revoke() noexcept
        {
            if (const auto operations = std::exchange(_operations, nullptr))
            {
                alignas(uint64_t) std::byte oldStorage[_storageSize];
                operations->move(&oldStorage, &_storage);
                operations->revoke(&oldStorage);
            }
        }

        void swap(event_revoker& other) noexcept
        {
            if (this != &other)
            {
                event_revoker temporary{ std::move(other) };
                other._move_from(std::move(*this));
                _move_from(std::move(temporary));
            }
        }

        explicit operator bool() const noexcept
        {
            return _operations && _operations->has_value(&_storage);
        }

    private:
        struct Operations
        {
            void (*revoke)(void*) noexcept;
            void (*move)(void*, void*) noexcept;
            bool (*has_value)(const void*) noexcept;
        };

        template<typename T>
        static const Operations* _get_operations() noexcept
        {
            static constexpr Operations operations{
                [](void* storage) noexcept {
                    auto value = std::launder(reinterpret_cast<T*>(storage));
                    value->revoke();
                    std::destroy_at(value);
                },
                [](void* destination, void* source) noexcept {
                    auto value = std::launder(reinterpret_cast<T*>(source));
                    std::construct_at(reinterpret_cast<T*>(destination), std::move(*value));
                    std::destroy_at(value);
                },
                [](const void* storage) noexcept {
                    return static_cast<bool>(*std::launder(reinterpret_cast<const T*>(storage)));
                },
            };
            return &operations;
        }

        template<typename T>
        void _emplace(T&& revoker) noexcept
        {
            using revoker_type = std::remove_cvref_t<T>;
            std::construct_at(reinterpret_cast<revoker_type*>(&_storage), std::forward<T>(revoker));
            _operations = _get_operations<revoker_type>();
        }

        void _move_from(event_revoker&& other) noexcept
        {
            if (const auto operations = std::exchange(other._operations, nullptr))
            {
                operations->move(&_storage, &other._storage);
                _operations = operations;
            }
        }

        alignas(uint64_t) std::byte _storage[_storageSize];
        const Operations* _operations = nullptr;
    };

    // Owns a collection of event revokers that all share a lifetime. This is
    // useful when individual subscriptions never need to be revoked early.
    class event_revoker_set
    {
    public:
        template<typename Source, typename Owner>
        class handler_binder
        {
        public:
            handler_binder(event_revoker_set& revokers, const Source& source, Owner* owner) :
                _revokers{ revokers },
                _source{ source },
                _owner{ owner->get_weak() }
            {
            }

            handler_binder(const handler_binder&) = delete;
            handler_binder& operator=(const handler_binder&) = delete;
            handler_binder(handler_binder&&) = delete;
            handler_binder& operator=(handler_binder&&) = delete;

            template<typename EventBase, typename Revoker, typename Delegate, typename Handler>
                requires std::is_member_function_pointer_v<Handler>
            void add_handler(Revoker (EventBase::*event)(winrt::auto_revoke_t, const Delegate&) const,
                             Handler handler)
            {
                _revokers.add(event_handler(_source, event, winrt::weak_ref<Owner>{ _owner }, std::move(handler)));
            }

        private:
            event_revoker_set& _revokers;
            Source _source;
            winrt::weak_ref<Owner> _owner;
        };

        event_revoker_set() noexcept = default;

        event_revoker_set(event_revoker_set&& other) noexcept
        {
            _revokers.swap(other._revokers);
        }

        event_revoker_set& operator=(event_revoker_set&& other) noexcept
        {
            if (this != &other)
            {
                event_revoker_set{ std::move(other) }._revokers.swap(_revokers);
            }
            return *this;
        }

        event_revoker_set(const event_revoker_set&) = delete;
        event_revoker_set& operator=(const event_revoker_set&) = delete;

        ~event_revoker_set() noexcept
        {
            revoke();
        }

        template<typename T>
        void add(T&& revoker)
        {
            _revokers.emplace_back(std::forward<T>(revoker));
        }

        template<typename Source, typename Owner>
        [[nodiscard]] auto bind(const Source& source, Owner* owner) &
        {
            return handler_binder<Source, Owner>{ *this, source, owner };
        }

        void revoke() noexcept
        {
            std::vector<event_revoker> revokers;
            revokers.swap(_revokers);
            while (!revokers.empty())
            {
                revokers.pop_back();
            }
        }

        explicit operator bool() const noexcept
        {
            return !_revokers.empty();
        }

    private:
        std::vector<event_revoker> _revokers;
    };

#endif
#ifdef WINRT_Windows_UI_Xaml_Data_H

    using property_changed_event = til::event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler>;
    // Making a til::observable_property unfortunately doesn't seem feasible.
    // It's gonna just result in more macros, which no one wants.
    //
    // 1. We don't know who the sender is, or would require `this` to always be
    //    the first parameter to one of these observable_property's.
    //
    // 2. We don't know what our own name is. We need to actually raise an event
    //    with the name of the variable as the parameter. Only way to do that is
    //    with something  like
    //
    //        til::observable<int> Foo(this, L"Foo", 42)
    //
    //    which then kinda implies the creation of:
    //
    //        #define OBSERVABLE(type, name, ...) til::observable_property<type> name{ this, L## #name, this.PropertyChanged, __VA_ARGS__ };
    //
    //     Which is just silly

#endif

    struct transparent_hstring_hash
    {
        using is_transparent = void;

        size_t operator()(const auto& hstr) const noexcept
        {
            return std::hash<std::wstring_view>{}(hstr);
        }
    };

    struct transparent_hstring_equal_to
    {
        using is_transparent = void;

        bool operator()(const auto& lhs, const auto& rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

    // fmt::format but for HSTRING.
    //
    // NOTE: This will fail to compile if you pass a string literal as the first argument (the format argument).
    // This is because std::forwarding literals turns them from constant expressions into regular ones.
    // It can be fixed by giving the first argument an explicit type. I intentionally didn't do that
    // because if you pass a string literal, you really ought to pass a FMT_COMPILE() instead.
    winrt::hstring hstring_format(auto&&... args)
    {
        // We could use fmt::formatted_size and winrt::impl::hstring_builder here,
        // and this would make formatting of large strings a bit faster, and a bit slower
        // for short strings. More importantly, I hit compilation issues so I dropped that.
        fmt::basic_memory_buffer<wchar_t> buf;
        fmt::format_to(std::back_inserter(buf), std::forward<decltype(args)>(args)...);
        return winrt::hstring{ buf.data(), gsl::narrow<uint32_t>(buf.size()) };
    }
}

template<>
struct fmt::formatter<winrt::hstring, wchar_t> : fmt::formatter<fmt::wstring_view, wchar_t>
{
    auto format(const winrt::hstring& str, auto& ctx) const
    {
        return fmt::formatter<fmt::wstring_view, wchar_t>::format({ str.data(), str.size() }, ctx);
    }
};

template<>
struct fmt::formatter<winrt::guid, wchar_t> : fmt::formatter<fmt::wstring_view, wchar_t>
{
    auto format(const winrt::guid& value, auto& ctx) const
    {
        return fmt::format_to(
            ctx.out(),
            L"{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
            value.Data1,
            value.Data2,
            value.Data3,
            value.Data4[0],
            value.Data4[1],
            value.Data4[2],
            value.Data4[3],
            value.Data4[4],
            value.Data4[5],
            value.Data4[6],
            value.Data4[7]);
    }
};
