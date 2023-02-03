// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    namespace details
    {
        template<typename T>
        struct is_contiguous_view : std::false_type
        {
        };
        // std::span remains largely unused in this code base so far.
        //template<typename U, std::size_t E>
        //struct is_contiguous_view<std::span<U, E>> : std::true_type
        //{
        //};
        template<typename U, typename V>
        struct is_contiguous_view<std::basic_string_view<U, V>> : std::true_type
        {
        };
#ifdef GSL_SPAN_H
        template<typename U, std::size_t E>
        struct is_contiguous_view<std::span<U, E>> : std::true_type
        {
        };
#endif

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
    }

    template<typename T>
    concept Byte = details::is_byte<std::remove_cv_t<T>>::value;

    template<typename T>
    concept ContiguousView = details::is_contiguous_view<std::remove_cv_t<T>>::value;

    template<typename T>
    concept ContiguousBytes = ContiguousView<T> && Byte<typename T::value_type>;

    template<typename T>
    concept TriviallyCopyable = std::is_trivially_copyable_v<T>;
}
