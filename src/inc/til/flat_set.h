// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#pragma warning(push)
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).

namespace til
{
    // A simple hash function for simple hash maps.
    // As demonstrated in https://doi.org/10.14778/2850583.2850585, a simple "multiply and shift" hash performs
    // very well with linear probing hash maps and I found this to be true as well in my own testing. This hash
    // function doesn't do the "shift" part, because linear_flat_set already does it by an appropriate amount.
    constexpr size_t flat_set_hash_integer(size_t v) noexcept
    {
        // These two multipliers are the same as used by the PCG family of random number generators.
        // The 32-Bit version is described in https://doi.org/10.1090/S0025-5718-99-00996-5, Table 5.
        // The 64-Bit version is the multiplier as used by Donald Knuth for MMIX and found by C. E. Haynes.
#ifdef _WIN64
        return v * UINT64_C(6364136223846793005);
#else
        return v * UINT32_C(747796405);
#endif
    }

    // A basic, hashmap with linear probing. A `LoadFactor` of 2 equals
    // a max. load of roughly 50% and a `LoadFactor` of 4 roughly 25%.
    //
    // `GrowthExponent` controls how fast the set grows and corresponds to
    // a rate of 2^GrowthExponent. In other words, a `GrowthExponent` of 3
    // equals a growth rate of 8x every time the capacity has been reached.
    //
    // It performs best with:
    // * small and cheap T
    // * >= 50% successful lookups
    // * <= 50% load factor (LoadFactor >= 2, which is the minimum anyways)
    template<typename T, size_t LoadFactor = 2, size_t GrowthExponent = 1>
    struct linear_flat_set
    {
        static_assert(LoadFactor >= 2);
        static_assert(GrowthExponent >= 1);

        linear_flat_set() = default;

        linear_flat_set(const linear_flat_set&) = delete;
        linear_flat_set& operator=(const linear_flat_set&) = delete;

        linear_flat_set(linear_flat_set&& other) noexcept :
            _map{ std::move(other._map) },
            _capacity{ std::exchange(other._capacity, 0) },
            _load{ std::exchange(other._load, 0) },
            _shift{ std::exchange(other._shift, initialShift) },
            _mask{ std::exchange(other._mask, 0) }
        {
        }

        linear_flat_set& operator=(linear_flat_set&& other) noexcept
        {
            _map = std::move(other._map);
            _capacity = std::exchange(other._capacity, 0);
            _load = std::exchange(other._load, 0);
            _shift = std::exchange(other._shift, initialShift);
            _mask = std::exchange(other._mask, 0);
            return *this;
        }

        bool empty() const noexcept
        {
            return _load == 0;
        }

        size_t size() const noexcept
        {
            return _load / LoadFactor;
        }

        std::span<T> container() const noexcept
        {
            return { _map.get(), _capacity };
        }

        void clear() noexcept
        {
            if (_map)
            {
                std::fill_n(_map.get(), _capacity, T{});
                _load = 0;
            }
        }

        template<typename U>
        T* lookup(U&& key) const noexcept
        {
            if (!_map)
            {
                return nullptr;
            }

            const auto hash = ::std::hash<T>{}(key) >> _shift;

            for (auto i = hash;; ++i)
            {
                auto& slot = _map[i & _mask];
                if (!slot)
                {
                    return nullptr;
                }
                if (slot == key) [[likely]]
                {
                    return &slot;
                }
            }
        }

        template<typename U>
        std::pair<T&, bool> insert(U&& key)
        {
            // Putting this into the lookup path is a little pessimistic, but it
            // allows us to default-construct this hashmap with a size of 0.
            if (_load >= _capacity) [[unlikely]]
            {
                _bumpSize();
            }

            // The most common, basic and performant hash function is to multiply the value
            // by some prime number and divide by the number of slots. It's been shown
            // many times in literature that such a scheme performs the best on average.
            // As such, we perform the divide here to get the topmost bits down.
            // See flat_set_hash_integer.
            const auto hash = ::std::hash<T>{}(key) >> _shift;

            for (auto i = hash;; ++i)
            {
                auto& slot = _map[i & _mask];
                if (!slot)
                {
                    slot = std::forward<U>(key);
                    _load += LoadFactor;
                    return { slot, true };
                }
                if (slot == key) [[likely]]
                {
                    return { slot, false };
                }
            }
        }

    private:
        __declspec(noinline) void _bumpSize()
        {
            // For instance at a GrowthExponent of 1:
            // A _shift of 0 would result in a newShift of 0xfffff...
            // A _shift of 1 would result in a newCapacity of 0
            if (_shift <= GrowthExponent)
            {
                throw std::bad_array_new_length{};
            }

            const auto newShift = _shift - GrowthExponent;
            const auto newCapacity = size_t{ 1 } << (digits - newShift);
            const auto newMask = newCapacity - 1;
            auto newMap = std::make_unique<T[]>(newCapacity);

            // This mirrors the insert() function, but without the lookup part.
            for (auto& oldSlot : container())
            {
                if (!oldSlot)
                {
                    continue;
                }

                const auto hash = ::std::hash<T>{}(oldSlot) >> newShift;

                for (auto i = hash;; ++i)
                {
                    auto& slot = newMap[i & newMask];
                    if (!slot)
                    {
                        slot = std::move_if_noexcept(oldSlot);
                        break;
                    }
                }
            }

            _map = std::move(newMap);
            _capacity = newCapacity;
            _shift = newShift;
            _mask = newMask;
        }

        static constexpr auto digits = std::numeric_limits<size_t>::digits;
        // This results in an initial capacity of 8 items, independent of the LoadFactor.
        static constexpr auto initialShift = digits - LoadFactor - 1;

        std::unique_ptr<T[]> _map;
        size_t _capacity = 0;
        size_t _load = 0;
        size_t _shift = initialShift;
        size_t _mask = 0;
    };
}

#pragma warning(pop)
