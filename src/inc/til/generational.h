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

    // It can be costly, difficult, or often impossible to compare two
    // instances of a struct. This little helper can simplify this.
    //
    // The underlying idea is that changes in state occur much less often than
    // the amount of data that's being processed in between. As such, this
    // helper assumes that _any_ modification to the struct it wraps is a
    // state change. When you compare the modified instance with another
    // the comparison operator will then always return false. This makes
    // state changes potentially more costly, because more state might be
    // invalidated than was necessary, but on the other hand it makes both,
    // the code simpler and the fast-path (no state change) much faster.
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
        T _value{};
    };
}
