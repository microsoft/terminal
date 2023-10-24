// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// static_map implements a very simple std::map-like type
// that is entirely (compile-time-)constant after C++20.
// There is no requirement that keys be sorted, as it will
// use constexpr std::sort during construction.
//
// Use til::presorted_static_map and make certain that
// your pairs are sorted if you want to skip the std::sort.
// A failure to sort your keys will result in unusual
// runtime behavior, but no error messages will be
// generated.

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        struct unsorted_input_t : public std::false_type
        {
        };

        struct presorted_input_t : public std::true_type
        {
        };
    }

    template<typename K, typename V, size_t N, typename SortedInput = details::unsorted_input_t>
    class static_map
    {
    public:
        using const_iterator = typename std::array<std::pair<K, V>, N>::const_iterator;

        template<typename... Args>
        constexpr explicit static_map(Args&&... args) noexcept :
            _array{ { std::forward<Args>(args)... } }
        {
            static_assert(sizeof...(Args) == N);
            if constexpr (!SortedInput::value)
            {
                std::sort(_array.begin(), _array.end());
            }
        }

        [[nodiscard]] constexpr const_iterator find(const auto& key) const noexcept
        {
            const auto iter = std::partition_point(_array.begin(), _array.end(), [&](const auto& p) {
                return p.first < key;
            });

            if (iter == _array.end() || key != iter->first)
            {
                return _array.end();
            }

            return iter;
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept
        {
            return _array.end();
        }

        [[nodiscard]] constexpr const V& at(const auto& key) const
        {
            const auto iter{ find(key) };

            if (iter == end())
            {
                throw std::runtime_error("key not found");
            }

            return iter->second;
        }

        [[nodiscard]] constexpr const V& operator[](const K& key) const
        {
            return at(key);
        }

    private:
        std::array<std::pair<K, V>, N> _array;
    };

    template<typename K, typename V, size_t N>
    class presorted_static_map : public static_map<K, V, N, details::presorted_input_t>
    {
    public:
        template<typename... Args>
        constexpr explicit presorted_static_map(Args&&... args) noexcept :
            static_map<K, V, N, details::presorted_input_t>{ std::forward<Args>(args)... }
        {
        }
    };

    // this is a deduction guide that ensures two things:
    // 1. static_map's member types are all the same
    // 2. static_map's fourth template argument (otherwise undeduced) is how many pairs it contains
    template<typename First, typename... Rest>
    static_map(First, Rest...) -> static_map<std::conditional_t<std::conjunction_v<std::is_same<First, Rest>...>, typename First::first_type, void>, typename First::second_type, 1 + sizeof...(Rest)>;

    template<typename First, typename... Rest>
    presorted_static_map(First, Rest...) -> presorted_static_map<std::conditional_t<std::conjunction_v<std::is_same<First, Rest>...>, typename First::first_type, void>, typename First::second_type, 1 + sizeof...(Rest)>;
}
