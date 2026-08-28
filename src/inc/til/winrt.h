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

    // C++/WinRT generates a distinct revoker type for every event, which makes
    // storing them unnecessarily verbose. This move-only type erases those
    // types while preserving their normal RAII behavior.
    class event_revoker
    {
    public:
        event_revoker() noexcept = default;

        template<typename T>
            requires(!std::is_same_v<std::remove_cvref_t<T>, event_revoker>)
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
            requires(!std::is_same_v<std::remove_cvref_t<T>, event_revoker>)
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
        static constexpr size_t _storageSize = 2 * sizeof(uint64_t);

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
            static_assert(sizeof(revoker_type) <= _storageSize, "auto_revoke result is unexpectedly large");
            static_assert(alignof(revoker_type) <= alignof(uint64_t), "auto_revoke result is unexpectedly aligned");
            static_assert(std::is_nothrow_constructible_v<revoker_type, T>);
            static_assert(std::is_nothrow_move_constructible_v<revoker_type>);
            static_assert(std::is_nothrow_destructible_v<revoker_type>);
            static_assert(noexcept(std::declval<revoker_type&>().revoke()));
            static_assert(noexcept(static_cast<bool>(std::declval<const revoker_type&>())));

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

        void revoke() noexcept
        {
            while (!_revokers.empty())
            {
                std::vector<event_revoker> revokers;
                revokers.swap(_revokers);
                while (!revokers.empty())
                {
                    revokers.pop_back();
                }
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
