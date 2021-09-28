// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <bitset>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // By design, this class hides several methods in the std::bitset class
    // so they can be called with an enum parameter instead of a size_t, so
    // we need to disable the "hides a non-virtual function" warning.
#pragma warning(push)
#pragma warning(disable : 26434)

    // til::enumset is a subclass of std::bitset, storing a fixed size array of
    // boolean elements, the positions in the array being identified by values
    // from a given enumerated type. By default it holds the same number of
    // bits as a size_t value.
    template<typename Type, size_t Bits = std::numeric_limits<size_t>::digits>
    class enumset : public std::bitset<Bits>
    {
        using _base = std::bitset<Bits>;

    public:
        using reference = typename _base::reference;

        enumset() = default;

        // Method Description:
        // - Constructs a new bitset with the given list of positions set to true.
        template<typename... Args, typename = std::enable_if_t<std::conjunction_v<std::is_same<Type, Args>...>>>
        constexpr enumset(const Args... positions) noexcept :
            _base((... | (1ULL << static_cast<size_t>(positions))))
        {
        }

        // Method Description:
        // - Returns the value of the bit at the given position.
        constexpr bool operator[](const Type pos) const
        {
            return _base::operator[](static_cast<size_t>(pos));
        }

        // Method Description:
        // - Returns a reference to the bit at the given position.
        reference operator[](const Type pos)
        {
            return _base::operator[](static_cast<size_t>(pos));
        }

        // Method Description:
        // - Returns the value of the bit at the given position.
        //   Throws std::out_of_range if it is not a valid position
        //   in the bitset.
        bool test(const Type pos) const
        {
            return _base::test(static_cast<size_t>(pos));
        }

        // Method Description:
        // - Returns true if any of the bits are set to true.
        bool any() const noexcept
        {
            return _base::any();
        }

        // Method Description:
        // - Returns true if any of the bits in the given positions are true.
        template<typename... Args, typename = std::enable_if_t<std::conjunction_v<std::is_same<Type, Args>...>>>
        bool any(const Args... positions) const noexcept
        {
            return (enumset{ positions... } & *this) != 0;
        }

        // Method Description:
        // - Returns true if all of the bits are set to true.
        bool all() const noexcept
        {
            return _base::all();
        }

        // Method Description:
        // - Returns true if all of the bits in the given positions are true.
        template<typename... Args, typename = std::enable_if_t<std::conjunction_v<std::is_same<Type, Args>...>>>
        bool all(const Args... positions) const noexcept
        {
            return (enumset{ positions... } & *this) == enumset{ positions... };
        }

        // Method Description:
        // - Sets the bit in the given position to the specified value.
        enumset& set(const Type pos, const bool val = true)
        {
            _base::set(static_cast<size_t>(pos), val);
            return *this;
        }

        // Method Description:
        // - Resets the bit in the given position to false.
        enumset& reset(const Type pos)
        {
            _base::reset(static_cast<size_t>(pos));
            return *this;
        }

        // Method Description:
        // - Flips the bit at the given position.
        enumset& flip(const Type pos)
        {
            _base::flip(static_cast<size_t>(pos));
            return *this;
        }

        // Method Description:
        // - Sets all of the bits in the given positions to true.
        template<typename... Args>
        enumset& set_all(const Args... positions)
        {
            return (..., set(positions));
        }

        // Method Description:
        // - Resets all of the bits in the given positions to false.
        template<typename... Args>
        enumset& reset_all(const Args... positions)
        {
            return (..., reset(positions));
        }
    };
#pragma warning(pop)
}
