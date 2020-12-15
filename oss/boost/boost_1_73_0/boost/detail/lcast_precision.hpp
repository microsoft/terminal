// Copyright Alexander Nasonov & Paul A. Bristow 2006.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DETAIL_LCAST_PRECISION_HPP_INCLUDED
#define BOOST_DETAIL_LCAST_PRECISION_HPP_INCLUDED

#include <climits>
#include <ios>
#include <limits>

#include <boost/config.hpp>
#include <boost/integer_traits.hpp>

#ifndef BOOST_NO_IS_ABSTRACT
// Fix for SF:1358600 - lexical_cast & pure virtual functions & VC 8 STL
#include <boost/type_traits/conditional.hpp>
#include <boost/type_traits/is_abstract.hpp>
#endif

#if defined(BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS) || \
  (defined(BOOST_MSVC) && (BOOST_MSVC<1310))

#define BOOST_LCAST_NO_COMPILE_TIME_PRECISION
#endif

#ifdef BOOST_LCAST_NO_COMPILE_TIME_PRECISION
#include <boost/assert.hpp>
#else
#include <boost/static_assert.hpp>
#endif

namespace boost { namespace detail {

class lcast_abstract_stub {};

#ifndef BOOST_LCAST_NO_COMPILE_TIME_PRECISION
// Calculate an argument to pass to std::ios_base::precision from
// lexical_cast. See alternative implementation for broken standard
// libraries in lcast_get_precision below. Keep them in sync, please.
template<class T>
struct lcast_precision
{
#ifdef BOOST_NO_IS_ABSTRACT
    typedef std::numeric_limits<T> limits; // No fix for SF:1358600.
#else
    typedef BOOST_DEDUCED_TYPENAME boost::conditional<
        boost::is_abstract<T>::value
      , std::numeric_limits<lcast_abstract_stub>
      , std::numeric_limits<T>
      >::type limits;
#endif

    BOOST_STATIC_CONSTANT(bool, use_default_precision =
            !limits::is_specialized || limits::is_exact
        );

    BOOST_STATIC_CONSTANT(bool, is_specialized_bin =
            !use_default_precision &&
            limits::radix == 2 && limits::digits > 0
        );

    BOOST_STATIC_CONSTANT(bool, is_specialized_dec =
            !use_default_precision &&
            limits::radix == 10 && limits::digits10 > 0
        );

    BOOST_STATIC_CONSTANT(std::streamsize, streamsize_max =
            boost::integer_traits<std::streamsize>::const_max
        );

    BOOST_STATIC_CONSTANT(unsigned int, precision_dec = limits::digits10 + 1U);

    BOOST_STATIC_ASSERT(!is_specialized_dec ||
            precision_dec <= streamsize_max + 0UL
        );

    BOOST_STATIC_CONSTANT(unsigned long, precision_bin =
            2UL + limits::digits * 30103UL / 100000UL
        );

    BOOST_STATIC_ASSERT(!is_specialized_bin ||
            (limits::digits + 0UL < ULONG_MAX / 30103UL &&
            precision_bin > limits::digits10 + 0UL &&
            precision_bin <= streamsize_max + 0UL)
        );

    BOOST_STATIC_CONSTANT(std::streamsize, value =
            is_specialized_bin ? precision_bin
                               : is_specialized_dec ? precision_dec : 6
        );
};
#endif

template<class T>
inline std::streamsize lcast_get_precision(T* = 0)
{
#ifndef BOOST_LCAST_NO_COMPILE_TIME_PRECISION
    return lcast_precision<T>::value;
#else // Follow lcast_precision algorithm at run-time:

#ifdef BOOST_NO_IS_ABSTRACT
    typedef std::numeric_limits<T> limits; // No fix for SF:1358600.
#else
    typedef BOOST_DEDUCED_TYPENAME boost::conditional<
        boost::is_abstract<T>::value
      , std::numeric_limits<lcast_abstract_stub>
      , std::numeric_limits<T>
      >::type limits;
#endif

    bool const use_default_precision =
        !limits::is_specialized || limits::is_exact;

    if(!use_default_precision)
    { // Includes all built-in floating-point types, float, double ...
      // and UDT types for which digits (significand bits) is defined (not zero)

        bool const is_specialized_bin =
            limits::radix == 2 && limits::digits > 0;
        bool const is_specialized_dec =
            limits::radix == 10 && limits::digits10 > 0;
        std::streamsize const streamsize_max =
            (boost::integer_traits<std::streamsize>::max)();
        (void)streamsize_max;

        if(is_specialized_bin)
        { // Floating-point types with
          // limits::digits defined by the specialization.

            unsigned long const digits = limits::digits;
            unsigned long const precision = 2UL + digits * 30103UL / 100000UL;
            // unsigned long is selected because it is at least 32-bits
            // and thus ULONG_MAX / 30103UL is big enough for all types.
            BOOST_ASSERT(
                    digits < ULONG_MAX / 30103UL &&
                    precision > limits::digits10 + 0UL &&
                    precision <= streamsize_max + 0UL
                );
            return precision;
        }
        else if(is_specialized_dec)
        {   // Decimal Floating-point type, most likely a User Defined Type
            // rather than a real floating-point hardware type.
            unsigned int const precision = limits::digits10 + 1U;
            BOOST_ASSERT(precision <= streamsize_max + 0UL);
            return precision;
        }
    }

    // Integral type (for which precision has no effect)
    // or type T for which limits is NOT specialized,
    // so assume stream precision remains the default 6 decimal digits.
    // Warning: if your User-defined Floating-point type T is NOT specialized,
    // then you may lose accuracy by only using 6 decimal digits.
    // To avoid this, you need to specialize T with either
    // radix == 2 and digits == the number of significand bits,
    // OR
    // radix = 10 and digits10 == the number of decimal digits.

    return 6;
#endif
}

template<class T>
inline void lcast_set_precision(std::ios_base& stream, T*)
{
    stream.precision(lcast_get_precision<T>());
}

template<class Source, class Target>
inline void lcast_set_precision(std::ios_base& stream, Source*, Target*)
{
    std::streamsize const s = lcast_get_precision(static_cast<Source*>(0));
    std::streamsize const t = lcast_get_precision(static_cast<Target*>(0));
    stream.precision(s > t ? s : t);
}

}}

#endif //  BOOST_DETAIL_LCAST_PRECISION_HPP_INCLUDED

