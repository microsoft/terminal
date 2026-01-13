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
