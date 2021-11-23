// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef __cpp_concepts
#define TIL_ENUMSET_VARARG template<std::same_as<T>... Args>
#else
#define TIL_ENUMSET_VARARG template<typename... Args, typename = std::enable_if_t<std::conjunction_v<std::is_same<T, Args>...>>>
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // til::enumset is a subclass of std::bitset, storing a fixed size array of
    // boolean elements, the positions in the array being identified by values
    // from a given enumerated type. By default it holds the same number of
    // bits as a size_t value.
    template<typename T>
    class enumset
    {
        using underlying_type = uintptr_t;

    public:
        // Method Description:
        // - Constructs a new bitset with the given list of positions set to true.
        TIL_ENUMSET_VARARG
        constexpr enumset(Args... positions) noexcept :
            _data{ to_underlying(positions...) }
        {
        }

        underlying_type bits() const noexcept
        {
            return _data;
        }

        // Method Description:
        // - Returns the value of the bit at the given position.
        //   Throws std::out_of_range if it is not a valid position
        //   in the bitset.
        bool test(const T pos) const noexcept
        {
            const auto mask = to_underlying(pos);
            return (_data & mask) != 0;
        }

        // Method Description:
        // - Returns true if any of the bits are set to true.
        bool any() const noexcept
        {
            return _data != 0;
        }

        // Method Description:
        // - Returns true if any of the bits in the given positions are true.
        TIL_ENUMSET_VARARG
        bool any(Args... positions) const noexcept
        {
            const auto mask = to_underlying(positions...);
            return (_data & mask) != 0;
        }

        // Method Description:
        // - Returns true if all of the bits are set to true.
        bool all() const noexcept
        {
            return _data == ~underlying_type(0);
        }

        // Method Description:
        // - Returns true if all of the bits in the given positions are true.
        TIL_ENUMSET_VARARG
        bool all(Args... positions) const noexcept
        {
            const auto mask = to_underlying(positions...);
            return (_data & mask) == mask;
        }

        // Method Description:
        // - Sets all of the bits in the given positions to true.
        TIL_ENUMSET_VARARG
        constexpr enumset& set(Args... positions) noexcept
        {
            _data |= to_underlying(positions...);
            return *this;
        }

        // Method Description:
        // - Sets the bit in the given position to the specified value.
        constexpr enumset& set(const T pos, const bool val) noexcept
        {
            if (val)
            {
                set(pos);
            }
            else
            {
                reset(pos);
            }
            return *this;
        }

        // Method Description:
        // - Resets all of the bits in the given positions to false.
        TIL_ENUMSET_VARARG
        constexpr enumset& reset(Args... positions) noexcept
        {
            _data &= ~to_underlying(positions...);
            return *this;
        }

        // Method Description:
        // - Flips the bit at the given position.
        TIL_ENUMSET_VARARG
        constexpr enumset& flip(Args... positions) noexcept
        {
            _data ^= to_underlying(positions...);
            return *this;
        }

    private:
        template<typename... Args>
        static constexpr underlying_type to_underlying(Args... positions) noexcept
        {
            return ((underlying_type{ 1 } << static_cast<underlying_type>(positions)) | ...);
        }

        template<>
        static constexpr underlying_type to_underlying() noexcept
        {
            return 0;
        }

        underlying_type _data{};
    };
}
