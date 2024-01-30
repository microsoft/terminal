// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    // A copy of gsl::narrow_cast for forced, unchecked casts.
    template<typename T, typename U>
    [[gsl::suppress(type)]] constexpr T narrow_cast(U&& u) noexcept
    {
        return static_cast<T>(std::forward<U>(u));
    }

    // The til::math namespace contains TIL math guidance casts;
    // they are intended to be used as the first argument to
    // floating-point universal converters elsewhere in the til namespace.
    namespace math
    {
        namespace details
        {
            // Just like gsl::narrow, but also checks for NAN.
            template<typename O, typename T>
            constexpr O narrow_float(T val)
            {
                const auto o = narrow_cast<O>(val);
                THROW_HR_IF(INTSAFE_E_ARITHMETIC_OVERFLOW, static_cast<T>(o) != val);
                return o;
            }

            struct ceiling_t
            {
                template<typename O, typename T>
                static constexpr O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return narrow_float<O, T>(std::ceil(val));
                    }
                    else
                    {
                        return wil::safe_cast<O>(val);
                    }
                }
            };

            struct flooring_t
            {
                template<typename O, typename T>
                static constexpr O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return narrow_float<O, T>(std::floor(val));
                    }
                    else
                    {
                        return wil::safe_cast<O>(val);
                    }
                }
            };

            struct rounding_t
            {
                template<typename O, typename T>
                static constexpr O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        return narrow_float<O, T>(std::round(val));
                    }
                    else
                    {
                        return wil::safe_cast<O>(val);
                    }
                }
            };
        }

        static constexpr details::ceiling_t ceiling; // positives become more positive, negatives become less negative
        static constexpr details::flooring_t flooring; // positives become less positive, negatives become more negative
        static constexpr details::rounding_t rounding; // it's rounding, from math class
    }
}
