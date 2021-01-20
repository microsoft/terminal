//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Stephen Cleary 2000.
// (C) Copyright Ion Gaztanaga 2007-2013.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
// This file is a slightly modified file from Boost.Pool
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_MATH_FUNCTIONS_HPP
#define BOOST_CONTAINER_DETAIL_MATH_FUNCTIONS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <climits>
#include <boost/static_assert.hpp>

namespace boost {
namespace container {
namespace dtl {

// Greatest common divisor and least common multiple

//
// gcd is an algorithm that calculates the greatest common divisor of two
//  integers, using Euclid's algorithm.
//
// Pre: A > 0 && B > 0
// Recommended: A > B
template <typename Integer>
inline Integer gcd(Integer A, Integer B)
{
   do
   {
      const Integer tmp(B);
      B = A % B;
      A = tmp;
   } while (B != 0);

   return A;
}

//
// lcm is an algorithm that calculates the least common multiple of two
//  integers.
//
// Pre: A > 0 && B > 0
// Recommended: A > B
template <typename Integer>
inline Integer lcm(const Integer & A, const Integer & B)
{
   Integer ret = A;
   ret /= gcd(A, B);
   ret *= B;
   return ret;
}

template <typename Integer>
inline Integer log2_ceil(const Integer & A)
{
   Integer i = 0;
   Integer power_of_2 = 1;

   while(power_of_2 < A){
      power_of_2 <<= 1;
      ++i;
   }
   return i;
}

template <typename Integer>
inline Integer upper_power_of_2(const Integer & A)
{
   Integer power_of_2 = 1;

   while(power_of_2 < A){
      power_of_2 <<= 1;
   }
   return power_of_2;
}

template <typename Integer, bool Loop = true>
struct upper_power_of_2_loop_ct
{

   template <Integer I, Integer P>
   struct apply
   {
      static const Integer value =
         upper_power_of_2_loop_ct<Integer, (I > P*2)>::template apply<I, P*2>::value;
   };
};

template <typename Integer>
struct upper_power_of_2_loop_ct<Integer, false>
{
   template <Integer I, Integer P>
   struct apply
   {
      static const Integer value = P;
   };
};

template <typename Integer, Integer I>
struct upper_power_of_2_ct
{
   static const Integer value = upper_power_of_2_loop_ct<Integer, (I > 1)>::template apply<I, 2>::value;
};

//This function uses binary search to discover the
//highest set bit of the integer
inline std::size_t floor_log2 (std::size_t x)
{
   const std::size_t Bits = sizeof(std::size_t)*CHAR_BIT;
   const bool Size_t_Bits_Power_2= !(Bits & (Bits-1));
   BOOST_STATIC_ASSERT(((Size_t_Bits_Power_2)== true));

   std::size_t n = x;
   std::size_t log2 = 0;

   for(std::size_t shift = Bits >> 1; shift; shift >>= 1){
      std::size_t tmp = n >> shift;
      if (tmp)
         log2 += shift, n = tmp;
   }

   return log2;
}

template<std::size_t I1, std::size_t I2>
struct gcd_ct
{
   static const std::size_t Max = I1 > I2 ? I1 : I2;
   static const std::size_t Min = I1 < I2 ? I1 : I2;
   static const std::size_t value = gcd_ct<Min, Max % Min>::value;
};

template<std::size_t I1>
struct gcd_ct<I1, 0>
{
   static const std::size_t value = I1;
};

template<std::size_t I1>
struct gcd_ct<0, I1>
{
   static const std::size_t value = I1;
};

template<std::size_t I1, std::size_t I2>
struct lcm_ct
{
   static const std::size_t value = I1 * I2 / gcd_ct<I1, I2>::value;
};

} // namespace dtl
} // namespace container
} // namespace boost

#include <boost/container/detail/config_end.hpp>

#endif
