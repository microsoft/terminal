// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#pragma warning(push)
#pragma warning(suppress : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).

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

    template<typename T>
    struct flat_set_trait;

    // This is an example implementation for a linear_flat_set that can store any size_t != -1.
    // Apart from this trait, the only other thing the type T has to implement is a copy or move assignment operator.
    template<>
    struct flat_set_trait<size_t>
    {
        using T = size_t;

        static size_t hash(T v) noexcept
        {
            return flat_set_hash_integer(v);
        }

        // Return true if the key and existing slot in the hashmap match.
        static bool equals(T slot, T key)
        {
            return slot == key;
        }

        // Return true if this slot can be filled with a new item.
        static bool empty(T slot)
        {
            return slot == -1;
        }

        // Called when a new item is inserted into the hashmap.
        // T::operator=(T&&) is called when the map is resized and existing items must be moved over.
        static void fill(T& slot, T key)
        {
            slot = key;
        }

        // Called when a new backing buffer is allocated. You need to then initialize the raw memory.
        static std::unique_ptr<T[]> allocate(size_t capacity)
        {
            return std::unique_ptr<T[]>{ new T[capacity]{ size_t(-1) } };
        }

        static void clear(T* data, size_t capacity) noexcept
        {
            for (auto& slot : std::span{ data, capacity })
            {
                slot = size_t(-1);
            }
        }
    };

    // A basic, hashmap with linear probing. A `LoadFactor` of 2 equals
    // a max. load of roughly 50% and a `LoadFactor` of 4 roughly 25%.
    //
    // It performs best with:
    // * small and cheap T
    // * >= 50% successful lookups
    // * <= 50% load factor (LoadFactor >= 2, which is the minimum anyways)
    template<typename T, size_t LoadFactor = 2>
    struct linear_flat_set
    {
        using Trait = typename flat_set_trait<T>;

        static_assert(LoadFactor >= 2);

        bool empty() const noexcept
        {
            return _load == 0;
        }

        size_t load() const noexcept
        {
            return _load;
        }

        size_t size() const noexcept
        {
            return _load / LoadFactor;
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
            // As such, we perform the divide her to get the topmost bits down.
            const auto hash = Trait::hash(key) >> _shift;

            for (auto i = hash;; ++i)
            {
                auto& slot = _map[i & _mask];
                if (Trait::empty(slot))
                {
                    Trait::fill(slot, std::forward<U>(key));
                    _load += LoadFactor;
                    return { slot, true };
                }
                if (Trait::equals(slot, key)) [[likely]]
                {
                    return { slot, false };
                }
            }
        }

        void clear() noexcept
        {
            Trait::clear(_map.get(), _capacity);
            _load = 0;
        }

    private:
        __declspec(noinline) void _bumpSize()
        {
            if (!_shift)
            {
                throw std::bad_array_new_length{};
            }

            const auto newShift = _shift - 1;
            const auto newCapacity = size_t{ 1 } << (digits - newShift);
            const auto newMask = newCapacity - 1;
            auto newMap = Trait::allocate(newCapacity);

            // This mirrors the insert() function, but without the lookup part.
            for (auto& oldSlot : std::span{ _map.get(), _capacity })
            {
                if (Trait::empty(oldSlot))
                {
                    continue;
                }

                const auto hash = Trait::hash(oldSlot) >> newShift;

                for (auto i = hash;; ++i)
                {
                    auto& slot = newMap[i & newMask];
                    if (Trait::empty(slot))
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

        std::unique_ptr<T[]> _map;
        size_t _capacity = 0;
        size_t _load = 0;
        // This results in an initial capacity of 8 items, independent of the LoadFactor.
        size_t _shift = digits - LoadFactor - 1;
        size_t _mask = 0;
    };
}

#pragma warning(pop)
