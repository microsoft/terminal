//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_DETAIL_META_UTILS_CORE_HPP
#define BOOST_MOVE_DETAIL_META_UTILS_CORE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

//Small meta-typetraits to support move

namespace boost {
namespace move_detail {

template<typename T>
struct voider { typedef void type; };

//////////////////////////////////////
//             if_c
//////////////////////////////////////
template<bool C, typename T1, typename T2>
struct if_c
{
   typedef T1 type;
};

template<typename T1, typename T2>
struct if_c<false,T1,T2>
{
   typedef T2 type;
};

//////////////////////////////////////
//             if_
//////////////////////////////////////
template<typename T1, typename T2, typename T3>
struct if_ : if_c<0 != T1::value, T2, T3>
{};

//////////////////////////////////////
//          enable_if_c
//////////////////////////////////////
struct enable_if_nat{};

template <bool B, class T = enable_if_nat>
struct enable_if_c
{
   typedef T type;
};

template <class T>
struct enable_if_c<false, T> {};

//////////////////////////////////////
//           enable_if
//////////////////////////////////////
template <class Cond, class T = enable_if_nat>
struct enable_if : enable_if_c<Cond::value, T> {};

//////////////////////////////////////
//          disable_if_c
//////////////////////////////////////
template <bool B, class T = enable_if_nat>
struct disable_if_c
   : enable_if_c<!B, T>
{};

//////////////////////////////////////
//          disable_if
//////////////////////////////////////
template <class Cond, class T = enable_if_nat>
struct disable_if : enable_if_c<!Cond::value, T> {};

//////////////////////////////////////
//          integral_constant
//////////////////////////////////////
template<class T, T v>
struct integral_constant
{
   static const T value = v;
   typedef T value_type;
   typedef integral_constant<T, v> type;

     operator T() const { return value; }
   T operator()() const { return value; }
};

typedef integral_constant<bool, true >  true_type;
typedef integral_constant<bool, false > false_type;


//////////////////////////////////////
//             is_same
//////////////////////////////////////
template<class T, class U>
struct is_same
{
   static const bool value = false;
};
 
template<class T>
struct is_same<T, T>
{
   static const bool value = true;
};

//////////////////////////////////////
//        enable_if_same
//////////////////////////////////////
template <class T, class U, class R = enable_if_nat>
struct enable_if_same : enable_if<is_same<T, U>, R> {};

//////////////////////////////////////
//        disable_if_same
//////////////////////////////////////
template <class T, class U, class R = enable_if_nat>
struct disable_if_same : disable_if<is_same<T, U>, R> {};

}  //namespace move_detail {
}  //namespace boost {

#endif //#ifndef BOOST_MOVE_DETAIL_META_UTILS_CORE_HPP
