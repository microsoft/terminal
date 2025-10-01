// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt
{
    struct hstring;
}

namespace til
{
    namespace details
    {
        template<typename T>
        struct is_contiguous_view : std::false_type
        {
        };
        template<typename U, std::size_t E>
        struct is_contiguous_view<std::span<U, E>> : std::true_type
        {
        };
        template<typename U, typename V>
        struct is_contiguous_view<std::basic_string_view<U, V>> : std::true_type
        {
        };

        template<typename T>
        struct is_byte : std::false_type
        {
        };
        template<>
        struct is_byte<char> : std::true_type
        {
        };
        template<>
        struct is_byte<std::byte> : std::true_type
        {
        };

        template<typename T>
        struct as_view
        {
            using type = T;
        };

        template<typename T, typename Alloc>
        struct as_view<std::vector<T, Alloc>>
        {
            using type = std::span<const T>;
        };

        template<typename T, typename Traits, typename Alloc>
        struct as_view<std::basic_string<T, Traits, Alloc>>
        {
            using type = std::basic_string_view<T, Traits>;
        };

        template<>
        struct as_view<winrt::hstring>
        {
            using type = std::wstring_view;
        };
    }

    template<typename T>
    concept Byte = details::is_byte<std::remove_cv_t<T>>::value;

    template<typename T>
    concept ContiguousView = details::is_contiguous_view<std::remove_cv_t<T>>::value;

    template<typename T>
    concept ContiguousBytes = ContiguousView<T> && Byte<typename T::value_type>;

    template<typename T>
    concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

    template<typename T>
    using as_view_t = typename details::as_view<T>::type;
}
