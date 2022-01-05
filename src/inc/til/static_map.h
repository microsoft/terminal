// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// static_map implements a very simple std::map-like type
// that is entirely (compile-time-)constant after C++20.
// There is no requirement that keys be sorted, as it will
// use constexpr std::sort during construction.
//
// Until we can use C++20, this is no cheaper than using
// a static std::unordered_map that is initialized at
// startup or on first use.
// To build something that can be constexpr as of C++17,
// use til::presorted_static_map and make certain that
// your pairs are sorted as they would have been sorted
// by your comparator.
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

    template<typename K, typename V, typename Compare = std::less<K>, size_t N = 0, typename SortedInput = details::unsorted_input_t>
    class static_map
    {
    public:
        using const_iterator = typename std::array<std::pair<K, V>, N>::const_iterator;

        template<typename... Args>
        constexpr explicit static_map(const Args&... args) noexcept :
            _predicate{},
            _array{ { args... } }
        {
            static_assert(sizeof...(Args) == N);
            if constexpr (!SortedInput::value)
            {
                const auto compareKeys = [&](const auto& p1, const auto& p2) { return _predicate(p1.first, p2.first); };
                std::sort(_array.begin(), _array.end(), compareKeys); // compile-time sorting starting C++20
            }
        }

        [[nodiscard]] constexpr const_iterator find(const K& key) const noexcept
        {
            const auto compareKey = [&](const auto& p) { return _predicate(p.first, key); };
            const auto iter{ std::partition_point(_array.begin(), _array.end(), compareKey) };

            if (iter == _array.end() || _predicate(key, iter->first))
            {
                return _array.end();
            }

            return iter;
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept
        {
            return _array.end();
        }

        [[nodiscard]] constexpr const V& at(const K& key) const
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
        Compare _predicate;
        std::array<std::pair<K, V>, N> _array;
    };

    template<typename K, typename V, typename Compare = std::less<K>, size_t N = 0>
    class presorted_static_map : public static_map<K, V, Compare, N, details::presorted_input_t>
    {
    public:
        template<typename... Args>
        constexpr explicit presorted_static_map(const Args&... args) noexcept :
            static_map<K, V, Compare, N, details::presorted_input_t>{ args... } {};
    };

    // this is a deduction guide that ensures two things:
    // 1. static_map's member types are all the same
    // 2. static_map's fourth template argument (otherwise undeduced) is how many pairs it contains
    template<typename First, typename... Rest>
    static_map(First, Rest...) -> static_map<std::conditional_t<std::conjunction_v<std::is_same<First, Rest>...>, typename First::first_type, void>, typename First::second_type, std::less<typename First::first_type>, 1 + sizeof...(Rest), details::unsorted_input_t>;

    template<typename First, typename... Rest>
    presorted_static_map(First, Rest...) -> presorted_static_map<std::conditional_t<std::conjunction_v<std::is_same<First, Rest>...>, typename First::first_type, void>, typename First::second_type, std::less<typename First::first_type>, 1 + sizeof...(Rest)>;
}
