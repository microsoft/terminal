// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    template<typename T>
    struct hash_trait;

    struct hasher
    {
        explicit constexpr hasher(size_t state = FNV_offset_basis) noexcept :
            _hash{ state } {}

        template<typename T>
        constexpr void write(const T& v) noexcept
        {
            hash_trait<T>{}(*this, v);
        }

        template<typename T, typename = std::enable_if_t<std::has_unique_object_representations_v<T>>>
        constexpr void write(const T* data, size_t count) noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            write(reinterpret_cast<const uint8_t*>(data), sizeof(T) * count);
        }

#pragma warning(suppress : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
        constexpr void write(const uint8_t* data, size_t count) noexcept
        {
            for (size_t i = 0; i < count; ++i)
            {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
                _hash ^= static_cast<size_t>(data[i]);
                _hash *= FNV_prime;
            }
        }

        constexpr size_t finalize() const noexcept
        {
            return _hash;
        }

    private:
#if defined(_WIN64)
        static constexpr size_t FNV_offset_basis = 14695981039346656037ULL;
        static constexpr size_t FNV_prime = 1099511628211ULL;
#else
        static constexpr size_t FNV_offset_basis = 2166136261U;
        static constexpr size_t FNV_prime = 16777619U;
#endif

        size_t _hash = FNV_offset_basis;
    };

    namespace details
    {
        template<typename T, bool enable>
        struct conditionally_enabled_hash_trait
        {
            constexpr void operator()(hasher& h, const T& v) const noexcept
            {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
                h.write(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
            }
        };

        template<typename T>
        struct conditionally_enabled_hash_trait<T, false>
        {
            conditionally_enabled_hash_trait() = delete;
            conditionally_enabled_hash_trait(const conditionally_enabled_hash_trait&) = delete;
            conditionally_enabled_hash_trait(conditionally_enabled_hash_trait&&) = delete;
            conditionally_enabled_hash_trait& operator=(const conditionally_enabled_hash_trait&) = delete;
            conditionally_enabled_hash_trait& operator=(conditionally_enabled_hash_trait&&) = delete;
        };
    }

    template<typename T>
    struct hash_trait : details::conditionally_enabled_hash_trait<T, std::has_unique_object_representations_v<T>>
    {
    };

    template<>
    struct hash_trait<float>
    {
        constexpr void operator()(hasher& h, float v) const noexcept
        {
            v = v == 0.0f ? 0.0f : v; // map -0 to 0
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            h.write(reinterpret_cast<const uint8_t*>(&v), sizeof(v));
        }
    };

    template<>
    struct hash_trait<double>
    {
        constexpr void operator()(hasher& h, double v) const noexcept
        {
            v = v == 0.0 ? 0.0 : v; // map -0 to 0
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            h.write(reinterpret_cast<const uint8_t*>(&v), sizeof(v));
        }
    };

    template<typename T, typename CharTraits, typename Allocator>
    struct hash_trait<std::basic_string<T, CharTraits, Allocator>>
    {
        constexpr void operator()(hasher& h, const std::basic_string<T, CharTraits, Allocator>& v) const noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            h.write(reinterpret_cast<const uint8_t*>(v.data()), sizeof(T) * v.size());
        }
    };

    template<typename T, typename CharTraits>
    struct hash_trait<std::basic_string_view<T, CharTraits>>
    {
        constexpr void operator()(hasher& h, const std::basic_string_view<T, CharTraits>& v) const noexcept
        {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            h.write(reinterpret_cast<const uint8_t*>(v.data()), sizeof(T) * v.size());
        }
    };

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>>>
    constexpr size_t hash(const T v) noexcept
    {
        // This runs murmurhash3's finalizer (fmix32/fmix64) on a single integer.
        // It's fast, public domain and produces good results.
        auto h = static_cast<size_t>(v);
        if constexpr (sizeof(size_t) == 4)
        {
            h ^= h >> 16;
            h *= UINT32_C(0x85ebca6b);
            h ^= h >> 13;
            h *= UINT32_C(0xc2b2ae35);
            h ^= h >> 16;
        }
        else
        {
            h ^= h >> 33;
            h *= UINT64_C(0xff51afd7ed558ccd);
            h ^= h >> 33;
            h *= UINT64_C(0xc4ceb9fe1a85ec53);
            h ^= h >> 33;
        }
        return h;
    }

    template<typename T, typename = std::enable_if_t<!(std::is_integral_v<T> || std::is_enum_v<T>)>>
    constexpr size_t hash(const T& v) noexcept
    {
        hasher h;
        h.write(v);
        return h.finalize();
    }
}
