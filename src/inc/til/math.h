// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
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
                const auto o = gsl::narrow_cast<O>(val);
                if (static_cast<T>(o) != val)
                {
                    throw gsl::narrowing_error{};
                }
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
                        return gsl::narrow<O>(val);
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
                        return gsl::narrow<O>(val);
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
                        return gsl::narrow<O>(val);
                    }
                }
            };
        }

        static constexpr details::ceiling_t ceiling; // positives become more positive, negatives become less negative
        static constexpr details::flooring_t flooring; // positives become less positive, negatives become more negative
        static constexpr details::rounding_t rounding; // it's rounding, from math class
    }

    // This method has the same behavior as gsl::narrow<T>, but instead of throwing an
    // exception on narrowing failure it'll return false. On success it returns true.
    template<typename T, typename U>
    constexpr bool narrow_maybe(U u, T& out) noexcept
    {
        out = gsl::narrow_cast<T>(u);
        return static_cast<U>(out) == u && (std::is_signed_v<T> == std::is_signed_v<U> || (out < T{}) == (u < U{}));
    }
}
