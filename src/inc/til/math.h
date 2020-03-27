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
            struct ceiling_t
            {
                template<typename O, typename T>
                static O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        THROW_HR_IF(E_ABORT, ::std::isnan(val));
                        return ::base::saturated_cast<O>(::std::ceil(val));
                    }
                    else
                    {
                        return ::base::saturated_cast<O>(val);
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
                        THROW_HR_IF(E_ABORT, ::std::isnan(val));
                        return ::base::saturated_cast<O>(::std::floor(val));
                    }
                    else
                    {
                        return ::base::saturated_cast<O>(val);
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
                        THROW_HR_IF(E_ABORT, ::std::isnan(val));
                        return ::base::saturated_cast<O>(::std::round(val));
                    }
                    else
                    {
                        return ::base::saturated_cast<O>(val);
                    }
                }
            };

            struct truncating_t
            {
                template<typename O, typename T>
                static O cast(T val)
                {
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        THROW_HR_IF(E_ABORT, ::std::isnan(val));
                    }
                    return ::base::saturated_cast<O>(val);
                }
            };
        }

        static constexpr details::ceiling_t ceiling; // positives become more positive, negatives become less negative
        static constexpr details::flooring_t flooring; // positives become less positive, negatives become more negative
        static constexpr details::rounding_t rounding; // it's rounding, from math class
        static constexpr details::truncating_t truncating; // drop the decimal point, regardless of how close it is to the next value
    }
}
