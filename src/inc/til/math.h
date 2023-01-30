// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    namespace details
    {
        // Just like gsl::narrow, but specialized to throw INTSAFE_E_ARITHMETIC_OVERFLOW instead.
        template<typename T, typename U>
        constexpr bool narrow_failed(T t, U u)
        {
            return static_cast<U>(t) != u || (std::is_signed_v<T> != std::is_signed_v<U> && (t < T{}) != (u < U{}));
        }
    }

    template<typename T, typename U>
    constexpr T unsafe_cast(U u)
    {
        return static_cast<T>(u);
    }

    template<typename T, typename U>
    T safe_cast(U u)
    {
        if constexpr (std::is_floating_point_v<U>)
        {
            const auto t = unsafe_cast<T>(u);
            if (details::narrow_failed(t, u))
            {
                THROW_HR(INTSAFE_E_ARITHMETIC_OVERFLOW);
            }
            return t;
        }
        else
        {
            return wil::safe_cast<T>(u);
        }
    }

    template<typename T, typename U>
    T safe_cast_failfast(U u) noexcept
    {
        if constexpr (std::is_floating_point_v<U>)
        {
            const auto t = unsafe_cast<T>(u);
            if (details::narrow_failed(t, u))
            {
                FAIL_FAST_HR(INTSAFE_E_ARITHMETIC_OVERFLOW);
            }
            return t;
        }
        else
        {
            return wil::safe_cast_failfast<T>(u);
        }
    }

    template<typename T, typename U>
    T safe_cast_nothrow(U u) noexcept
    {
        return wil::safe_cast_nothrow<T>(u);
    }

    template<typename T, typename U>
    HRESULT safe_cast_nothrow(U u, T* pt) noexcept
    {
        if constexpr (std::is_floating_point_v<U>)
        {
            const auto t = unsafe_cast<T>(u);
            if (details::narrow_failed(t, u))
            {
                *pt = {};//NAN;
                return INTSAFE_E_ARITHMETIC_OVERFLOW;
            }
            *pt = t;
            return S_OK;
        }
        else
        {
            return wil::safe_cast_nothrow<T>(u, pt);
        }
    }

    // The til::math namespace contains TIL math guidance casts;
    // they are intended to be used as the first argument to
    // floating-point universal converters elsewhere in the til namespace.
    namespace math
    {
        namespace details
        {
            struct ceiling_t
            {
                template<typename O, typename T>
                static O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return safe_cast<O>(std::ceil(val));
                    }
                    else
                    {
                        return safe_cast<O>(val);
                    }
                }
            };

            struct flooring_t
            {
                template<typename O, typename T>
                static O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return safe_cast<O>(std::floor(val));
                    }
                    else
                    {
                        return safe_cast<O>(val);
                    }
                }
            };

            struct rounding_t
            {
                template<typename O, typename T>
                static O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return safe_cast<O>(std::round(val));
                    }
                    else
                    {
                        return safe_cast<O>(val);
                    }
                }
            };
        }

        static constexpr details::ceiling_t ceiling; // positives become more positive, negatives become less negative
        static constexpr details::flooring_t flooring; // positives become less positive, negatives become more negative
        static constexpr details::rounding_t rounding; // it's rounding, from math class
    }
}
