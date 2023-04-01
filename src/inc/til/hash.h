// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#pragma warning(push)
// std::hash() doesn't test for `nullptr`, nor do we want to.
#pragma warning(disable : 26429) // Symbol '...' is never tested for nullness, it can be marked as not_null (f.23).
// Misdiagnosis: static_cast<const void*> is used to differentiate between 2 overloads of til::hasher::write.
#pragma warning(disable : 26474) // Don't cast between pointer types when the conversion could be implicit (type.1).
// We don't want to unnecessarily modify wyhash from its original.
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26494) // Variable '...' is uninitialized. Always initialize an object (type.5).
#pragma warning(disable : 26496) // The variable '...' does not change after construction, mark it as const (con.4).

#if defined(_M_X64) && !defined(_M_ARM64EC)
#define TIL_HASH_X64
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
#define TIL_HASH_ARM64
#elif defined(_M_IX86) || defined(_M_ARM)
#define TIL_HASH_32BIT
#else
#error "Unsupported architecture for til::hash"
#endif

namespace til
{
    template<typename T>
    struct hash_trait;

    struct hasher
    {
        constexpr hasher() = default;
        explicit constexpr hasher(size_t state) noexcept :
            _hash{ state } {}

        template<typename T>
        hasher& write(const T& v) noexcept
        {
            hash_trait<T>{}(*this, v);
            return *this;
        }

        template<typename T, typename = std::enable_if_t<std::has_unique_object_representations_v<T>>>
        hasher& write(const T* data, size_t count) noexcept
        {
            return write(static_cast<const void*>(data), sizeof(T) * count);
        }

        hasher& write(const void* data, size_t len) noexcept
        {
            _hash = _wyhash(data, len, _hash);
            return *this;
        }

        constexpr size_t finalize() const noexcept
        {
            return _hash;
        }

    private:
#if defined(TIL_HASH_32BIT)

        static uint32_t _wyr24(const uint8_t* p, uint32_t k) noexcept
        {
            return static_cast<uint32_t>(p[0]) << 16 | static_cast<uint32_t>(p[k >> 1]) << 8 | p[k - 1];
        }

        static uint32_t _wyr32(const uint8_t* p) noexcept
        {
            uint32_t v;
            memcpy(&v, p, 4);
            return v;
        }

        static void _wymix32(uint32_t* a, uint32_t* b) noexcept
        {
            uint64_t c = *a ^ UINT32_C(0x53c5ca59);
            c *= *b ^ UINT32_C(0x74743c1b);
            *a = static_cast<uint32_t>(c);
            *b = static_cast<uint32_t>(c >> 32);
        }

        static uint32_t _wyhash(const void* data, uint32_t len, uint32_t seed) noexcept
        {
            auto p = static_cast<const uint8_t*>(data);
            auto i = len;
            auto see1 = len;
            _wymix32(&seed, &see1);

            for (; i > 8; i -= 8, p += 8)
            {
                seed ^= _wyr32(p);
                see1 ^= _wyr32(p + 4);
                _wymix32(&seed, &see1);
            }
            if (i >= 4)
            {
                seed ^= _wyr32(p);
                see1 ^= _wyr32(p + i - 4);
            }
            else if (i)
            {
                seed ^= _wyr24(p, i);
            }

            _wymix32(&seed, &see1);
            _wymix32(&seed, &see1);
            return seed ^ see1;
        }

#else // defined(TIL_HASH_32BIT)

        static uint64_t _wyr3(const uint8_t* p, size_t k) noexcept
        {
            return (static_cast<uint64_t>(p[0]) << 16) | (static_cast<uint64_t>(p[k >> 1]) << 8) | p[k - 1];
        }

        static uint64_t _wyr4(const uint8_t* p) noexcept
        {
            uint32_t v;
            memcpy(&v, p, 4);
            return v;
        }

        static uint64_t _wyr8(const uint8_t* p) noexcept
        {
            uint64_t v;
            memcpy(&v, p, 8);
            return v;
        }

        static uint64_t _wymix(uint64_t lhs, uint64_t rhs) noexcept
        {
#if defined(TIL_HASH_X64)
            uint64_t hi;
            uint64_t lo = _umul128(lhs, rhs, &hi);
#elif defined(TIL_HASH_ARM64)
            const uint64_t lo = lhs * rhs;
            const uint64_t hi = __umulh(lhs, rhs);
#endif
            return lo ^ hi;
        }

        static uint64_t _wyhash(const void* data, uint64_t len, uint64_t seed) noexcept
        {
            static constexpr auto s0 = UINT64_C(0xa0761d6478bd642f);
            static constexpr auto s1 = UINT64_C(0xe7037ed1a0b428db);
            static constexpr auto s2 = UINT64_C(0x8ebc6af09c88c6e3);
            static constexpr auto s3 = UINT64_C(0x589965cc75374cc3);

            auto p = static_cast<const uint8_t*>(data);
            seed ^= s0;
            uint64_t a;
            uint64_t b;

            if (len <= 16)
            {
                if (len >= 4)
                {
                    a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
                    b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
                }
                else if (len > 0)
                {
                    a = _wyr3(p, len);
                    b = 0;
                }
                else
                {
                    a = b = 0;
                }
            }
            else
            {
                auto i = len;
                if (i > 48)
                {
                    auto seed1 = seed;
                    auto seed2 = seed;
                    do
                    {
                        seed = _wymix(_wyr8(p) ^ s1, _wyr8(p + 8) ^ seed);
                        seed1 = _wymix(_wyr8(p + 16) ^ s2, _wyr8(p + 24) ^ seed1);
                        seed2 = _wymix(_wyr8(p + 32) ^ s3, _wyr8(p + 40) ^ seed2);
                        p += 48;
                        i -= 48;
                    } while (i > 48);
                    seed ^= seed1 ^ seed2;
                }
                while (i > 16)
                {
                    seed = _wymix(_wyr8(p) ^ s1, _wyr8(p + 8) ^ seed);
                    i -= 16;
                    p += 16;
                }
                a = _wyr8(p + i - 16);
                b = _wyr8(p + i - 8);
            }

            return _wymix(s1 ^ len, _wymix(a ^ s1, b ^ seed));
        }

#endif // defined(TIL_HASH_32BIT)

        size_t _hash = 0;
    };

    namespace details
    {
        template<typename T, bool enable>
        struct conditionally_enabled_hash_trait
        {
            void operator()(hasher& h, const T& v) const noexcept
            {
                h.write(static_cast<const void*>(&v), sizeof(T));
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
        void operator()(hasher& h, float v) const noexcept
        {
            v = v == 0.0f ? 0.0f : v; // map -0 to 0
            h.write(static_cast<const void*>(&v), sizeof(v));
        }
    };

    template<>
    struct hash_trait<double>
    {
        void operator()(hasher& h, double v) const noexcept
        {
            v = v == 0.0 ? 0.0 : v; // map -0 to 0
            h.write(static_cast<const void*>(&v), sizeof(v));
        }
    };

    template<typename T, typename CharTraits, typename Allocator>
    struct hash_trait<std::basic_string<T, CharTraits, Allocator>>
    {
        void operator()(hasher& h, const std::basic_string<T, CharTraits, Allocator>& v) const noexcept
        {
            h.write(v.data(), v.size());
        }
    };

    template<typename T, typename CharTraits>
    struct hash_trait<std::basic_string_view<T, CharTraits>>
    {
        void operator()(hasher& h, const std::basic_string_view<T, CharTraits>& v) const noexcept
        {
            h.write(v.data(), v.size());
        }
    };

    template<typename T>
    size_t hash(const T& v) noexcept
    {
        hasher h;
        h.write(v);
        return h.finalize();
    }

    inline size_t hash(const void* data, size_t len) noexcept
    {
        hasher h;
        h.write(data, len);
        return h.finalize();
    }
}

#pragma warning(pop)
