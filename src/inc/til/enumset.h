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
    // til::enumset stores a fixed size array of boolean elements, the positions
    // in the array being identified by values from a given enumerated type.
    // Position N corresponds to bit 1<<N in the UnderlyingType integer.
    //
    // If you only need 32 positions for your T, UnderlyingType can be set uint32_t.
    // It defaults to uintptr_t allowing you to set as many positions as a pointer has bits.
    // This class doesn't statically assert that your given position fits into UnderlyingType.
    template<typename T, typename UnderlyingType = uintptr_t>
    class enumset
    {
        static_assert(std::is_unsigned_v<UnderlyingType>);

    public:
        // Method Description:
        // - Constructs a new bitset with the given list of positions set to true.
        TIL_ENUMSET_VARARG
        constexpr enumset(Args... positions) noexcept :
            _data{ to_underlying(positions...) }
        {
        }

        // Method Description:
        // - Returns the underlying bit positions as a copy.
        constexpr UnderlyingType bits() const noexcept
        {
            return _data;
        }

        // Method Description:
        // - Returns the value of the bit at the given position.
        //   Throws std::out_of_range if it is not a valid position
        //   in the bitset.
        constexpr bool test(const T pos) const noexcept
        {
            const auto mask = to_underlying(pos);
            return (_data & mask) != 0;
        }

        // Method Description:
        // - Returns true if any of the bits are set to true.
        constexpr bool any() const noexcept
        {
            return _data != 0;
        }

        // Method Description:
        // - Returns true if any of the bits in the given positions are true.
        TIL_ENUMSET_VARARG
        constexpr bool any(Args... positions) const noexcept
        {
            const auto mask = to_underlying(positions...);
            return (_data & mask) != 0;
        }

        // Method Description:
        // - Returns true if all of the bits are set to true.
        constexpr bool all() const noexcept
        {
            return _data == ~UnderlyingType{ 0 };
        }

        // Method Description:
        // - Returns true if all of the bits in the given positions are true.
        TIL_ENUMSET_VARARG
        constexpr bool all(Args... positions) const noexcept
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
            const auto mask = to_underlying(pos);
            // false == 0 --> UnderlyingType(-0) == 0b0000...
            // true == 1  --> UnderlyingType(-1) == 0b1111...
#pragma warning(suppress : 4804) // '-': unsafe use of type 'bool' in operation
            _data = (_data & ~mask) | (-val & mask);
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
        static constexpr UnderlyingType to_underlying(Args... positions) noexcept
        {
            if constexpr (sizeof...(positions) == 0)
            {
                return 0;
            }
            else
            {
                return ((UnderlyingType{ 1 } << static_cast<UnderlyingType>(positions)) | ...);
            }
        }

        UnderlyingType _data{};
    };
}
