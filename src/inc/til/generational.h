// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    struct generation_t
    {
        auto operator<=>(const generation_t&) const = default;

        constexpr void bump() noexcept
        {
            _value++;
        }

        uint32_t _value = 0;
    };

    template<typename T>
    struct generational
    {
        generational() = default;
        explicit constexpr generational(auto&&... args) :
            _value{ std::forward<decltype(args)>(args)... } {}
        explicit constexpr generational(generation_t generation, auto&&... args) :
            _generation{ generation },
            _value{ std::forward<decltype(args)>(args)... } {}

        constexpr bool operator==(const generational& rhs) const noexcept { return generation() == rhs.generation(); }
        constexpr bool operator!=(const generational& rhs) const noexcept { return generation() != rhs.generation(); }

        constexpr generation_t generation() const noexcept
        {
            return _generation;
        }

        [[nodiscard]] constexpr const T* operator->() const noexcept
        {
            return &_value;
        }

        [[nodiscard]] constexpr const T& operator*() const noexcept
        {
            return _value;
        }

        [[nodiscard]] constexpr T* write() noexcept
        {
            _generation.bump();
            return &_value;
        }

    private:
        generation_t _generation;
        T _value;
    };
}
