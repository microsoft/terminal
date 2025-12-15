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

    // Unlike winrt::event, this event will only call handlers once at most.
    // It's otherwise a copy of winrt::event's implementation.
    template<typename ArgsT>
    struct fused_event
    {
        using delegate_type = ArgsT;
        using delegate_array = winrt::com_ptr<winrt::impl::event_array<delegate_type>>;

        fused_event() = default;
        fused_event(const fused_event&) = delete;
        fused_event& operator=(const fused_event&) = delete;

        fused_event(fused_event&& other)
        {
            const winrt::slim_lock_guard change_guard{ other.m_change };
            if (!other.m_targets)
            {
                return;
            }
            const winrt::slim_lock_guard swap_guard{ other.m_swap };
            m_targets = std::move(other.m_targets);
        }

        fused_event& operator=(fused_event&& other)
        {
            if (this != &other)
            {
                const winrt::slim_lock_guard other_change_guard{ other.m_change };
                const winrt::slim_lock_guard other_swap_guard{ other.m_swap };
                const winrt::slim_lock_guard self_change_guard{ m_change };
                const winrt::slim_lock_guard self_swap_guard{ m_swap };
                m_targets = std::move(other.m_targets);
            }
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return m_targets != nullptr;
        }

        winrt::event_token operator()(const delegate_type& delegate)
        {
            return add_agile(winrt::impl::make_agile_delegate(delegate));
        }

        void operator()(const winrt::event_token token)
        {
            // Extends life of old targets array to release delegates outside of lock.
            delegate_array temp_targets;

            {
                const winrt::slim_lock_guard change_guard{ m_change };

                if (!m_targets)
                {
                    return;
                }

                uint32_t available_slots = m_targets->size() - 1;
                delegate_array new_targets;
                bool removed = false;

                if (available_slots == 0)
                {
                    if (get_token(*m_targets->begin()) == token)
                    {
                        removed = true;
                    }
                }
                else
                {
                    new_targets = winrt::impl::make_event_array<delegate_type>(available_slots);
                    auto new_iterator = new_targets->begin();

                    for (delegate_type const& element : *m_targets)
                    {
                        if (!removed && token == get_token(element))
                        {
                            removed = true;
                            continue;
                        }

                        if (available_slots == 0)
                        {
                            WINRT_ASSERT(!removed);
                            break;
                        }

                        *new_iterator = element;
                        ++new_iterator;
                        --available_slots;
                    }
                }

                if (removed)
                {
                    const winrt::slim_lock_guard swap_guard{ m_swap };
                    temp_targets = std::exchange(m_targets, std::move(new_targets));
                }
            }
        }

        template<typename... Arg>
        void raise(const Arg&... args)
        {
            delegate_array temp_targets;

            {
                const winrt::slim_lock_guard change_guard{ m_change };

                if (!m_targets)
                {
                    return;
                }

                const winrt::slim_lock_guard swap_guard{ m_swap };
                temp_targets = std::move(m_targets);
            }

            if (temp_targets)
            {
                for (const auto& element : *temp_targets)
                {
                    if (!winrt::impl::invoke(element, args...))
                    {
                        operator()(get_token(element));
                    }
                }
            }
        }

    private:
        WINRT_IMPL_NOINLINE winrt::event_token add_agile(delegate_type delegate)
        {
            winrt::event_token token;

            // Extends life of old targets array to release delegates outside of lock.
            delegate_array temp_targets;

            {
                const winrt::slim_lock_guard change_guard{ m_change };
                const auto size = !m_targets ? 0 : m_targets->size();
                auto new_targets = winrt::impl::make_event_array<delegate_type>(size + 1);

                if (m_targets)
                {
                    std::copy_n(m_targets->begin(), m_targets->size(), new_targets->begin());
                }

                new_targets->back() = std::move(delegate);
                token = get_token(new_targets->back());

                const winrt::slim_lock_guard swap_guard{ m_swap };
                temp_targets = std::exchange(m_targets, std::move(new_targets));
            }

            return token;
        }

        winrt::event_token get_token(delegate_type const& delegate) const noexcept
        {
            return winrt::event_token{ reinterpret_cast<int64_t>(WINRT_IMPL_EncodePointer(winrt::get_abi(delegate))) };
        }

        delegate_array m_targets;
        winrt::slim_mutex m_swap;
        winrt::slim_mutex m_change;
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
}
