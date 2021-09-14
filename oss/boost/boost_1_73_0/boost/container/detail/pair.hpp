//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_CONTAINER_DETAIL_PAIR_HPP
#define BOOST_CONTAINER_CONTAINER_DETAIL_PAIR_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <boost/static_assert.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/std_fwd.hpp>
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#  include <boost/container/detail/variadic_templates_tools.hpp>
#endif
#include <boost/move/adl_move_swap.hpp> //swap

#include <boost/intrusive/detail/minimal_pair_header.hpp>      //pair
#include <boost/move/utility_core.hpp>
#include <boost/move/detail/fwd_macros.hpp>

namespace boost {
namespace tuples {

struct null_type;

template <
  class T0, class T1, class T2,
  class T3, class T4, class T5,
  class T6, class T7, class T8,
  class T9>
class tuple;

}  //namespace tuples {
}  //namespace boost {

namespace boost {
namespace container {
namespace pair_impl {

template <class TupleClass>
struct is_boost_tuple
{
   static const bool value = false;
};

template <
  class T0, class T1, class T2,
  class T3, class T4, class T5,
  class T6, class T7, class T8,
  class T9>
struct is_boost_tuple< boost::tuples::tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> >
{
   static const bool value = true;
};

template<class Tuple>
struct disable_if_boost_tuple
   : boost::container::dtl::disable_if< is_boost_tuple<Tuple> >
{};

template<class T>
struct is_tuple_null
{
   static const bool value = false;
};

template<>
struct is_tuple_null<boost::tuples::null_type>
{
   static const bool value = true;
};

}}}

#if defined(BOOST_MSVC) && (_CPPLIB_VER == 520)
//MSVC 2010 tuple marker
namespace std { namespace tr1 { struct _Nil; }}
#elif defined(BOOST_MSVC) && (_CPPLIB_VER == 540)
//MSVC 2012 tuple marker
namespace std { struct _Nil; }
#endif


namespace boost {
namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

   template <int Dummy = 0>
   struct std_piecewise_construct_holder
   {
      static ::std::piecewise_construct_t *dummy;
   };

   template <int Dummy>
   ::std::piecewise_construct_t *std_piecewise_construct_holder<Dummy>::dummy =
      reinterpret_cast< ::std::piecewise_construct_t *>(0x01234);  //Avoid sanitizer errors on references to null pointers

typedef const std::piecewise_construct_t & piecewise_construct_t;

struct try_emplace_t{};

#else

//! The piecewise_construct_t struct is an empty structure type used as a unique type to
//! disambiguate used to disambiguate between different functions that take two tuple arguments.
typedef unspecified piecewise_construct_t;

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//! A instance of type
//! piecewise_construct_t
static piecewise_construct_t piecewise_construct = BOOST_CONTAINER_DOC1ST(unspecified, *std_piecewise_construct_holder<>::dummy);

///@cond

namespace dtl {

struct piecewise_construct_use
{
   //Avoid warnings of unused "piecewise_construct"
   piecewise_construct_use()
   {  (void)&::boost::container::piecewise_construct;   }
};

template <class T1, class T2>
struct pair;

template <class T>
struct is_pair
{
   static const bool value = false;
};

template <class T1, class T2>
struct is_pair< pair<T1, T2> >
{
   static const bool value = true;
};

template <class T1, class T2>
struct is_pair< std::pair<T1, T2> >
{
   static const bool value = true;
};

template <class T>
struct is_not_pair
{
   static const bool value = !is_pair<T>::value;
};

template <class T>
struct is_std_pair
{
   static const bool value = false;
};

template <class T1, class T2>
struct is_std_pair< std::pair<T1, T2> >
{
   static const bool value = true;
};

struct pair_nat;

template<typename T, typename U, typename V>
void get(T); //to enable ADL

///@endcond

#ifdef  _LIBCPP_DEPRECATED_ABI_DISABLE_PAIR_TRIVIAL_COPY_CTOR
//Libc++, in some versions, has an ABI breakage that needs some
//padding in dtl::pair, as "std::pair::first" is not at offset zero.
//See: https://reviews.llvm.org/D56357 for more information.
//
template <class T1, class T2, std::size_t N>
struct pair_padding
{
   char padding[N];
};

template <class T1, class T2>
struct pair_padding<T1, T2, 0>
{
};

template <class T1, class T2>
struct simple_pair
{
   T1 first;
   T2 second;
};

#endif

template <class T1, class T2>
struct pair
#ifdef  _LIBCPP_DEPRECATED_ABI_DISABLE_PAIR_TRIVIAL_COPY_CTOR
   : pair_padding<T1, T2, sizeof(std::pair<T1, T2>) - sizeof(simple_pair<T1, T2>)>
#endif
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(pair)

   public:
   typedef T1 first_type;
   typedef T2 second_type;

   T1 first;
   T2 second;

   //Default constructor
   pair()
      : first(), second()
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   //pair copy assignment
   pair(const pair& x)
      : first(x.first), second(x.second)
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   //pair move constructor
   pair(BOOST_RV_REF(pair) p)
      : first(::boost::move(p.first)), second(::boost::move(p.second))
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   template <class D, class S>
   pair(const pair<D, S> &p)
      : first(p.first), second(p.second)
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   template <class D, class S>
   pair(BOOST_RV_REF_BEG pair<D, S> BOOST_RV_REF_END p)
      : first(::boost::move(p.first)), second(::boost::move(p.second))
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   //pair from two values
   pair(const T1 &t1, const T2 &t2)
      : first(t1)
      , second(t2)
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   template<class U, class V>
   pair(BOOST_FWD_REF(U) u, BOOST_FWD_REF(V) v)
      : first(::boost::forward<U>(u))
      , second(::boost::forward<V>(v))
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   //And now compatibility with std::pair
   pair(const std::pair<T1, T2>& x)
      : first(x.first), second(x.second)
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   template <class D, class S>
   pair(const std::pair<D, S>& p)
      : first(p.first), second(p.second)
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   pair(BOOST_RV_REF_BEG std::pair<T1, T2> BOOST_RV_REF_END p)
      : first(::boost::move(p.first)), second(::boost::move(p.second))
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   template <class D, class S>
   pair(BOOST_RV_REF_BEG std::pair<D, S> BOOST_RV_REF_END p)
      : first(::boost::move(p.first)), second(::boost::move(p.second))
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
   template< class KeyType, class ...Args>
   pair(try_emplace_t, BOOST_FWD_REF(KeyType) k, Args && ...args)
      : first(boost::forward<KeyType>(k)), second(::boost::forward<Args>(args)...)\
   {
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
   }
   #else

   //piecewise construction from boost::tuple
   #define BOOST_PAIR_TRY_EMPLACE_CONSTRUCT_CODE(N)\
   template< class KeyType BOOST_MOVE_I##N BOOST_MOVE_CLASS##N > \
   pair( try_emplace_t, BOOST_FWD_REF(KeyType) k BOOST_MOVE_I##N BOOST_MOVE_UREF##N )\
      : first(boost::forward<KeyType>(k)), second(BOOST_MOVE_FWD##N)\
   {\
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));\
   }\
   //
   BOOST_MOVE_ITERATE_0TO9(BOOST_PAIR_TRY_EMPLACE_CONSTRUCT_CODE)
   #undef BOOST_PAIR_TRY_EMPLACE_CONSTRUCT_CODE

   #endif   //BOOST_NO_CXX11_VARIADIC_TEMPLATES

   //piecewise construction from boost::tuple
   #define BOOST_PAIR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE(N,M)\
   template< template<class, class, class, class, class, class, class, class, class, class> class BoostTuple \
            BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
   pair( piecewise_construct_t\
       , BoostTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,N),::boost::tuples::null_type)> p\
       , BoostTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,M),::boost::tuples::null_type)> q\
       , typename dtl::enable_if_c\
         < pair_impl::is_boost_tuple< BoostTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,N),::boost::tuples::null_type)> >::value &&\
           !(pair_impl::is_tuple_null<BOOST_MOVE_LAST_TARG##N>::value || pair_impl::is_tuple_null<BOOST_MOVE_LAST_TARGQ##M>::value) \
         >::type* = 0\
       )\
      : first(BOOST_MOVE_TMPL_GET##N), second(BOOST_MOVE_TMPL_GETQ##M)\
   { (void)p; (void)q;\
      BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));\
   }\
   //
   BOOST_MOVE_ITER2D_0TOMAX(9, BOOST_PAIR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE)
   #undef BOOST_PAIR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE

   //piecewise construction from variadic tuple (with delegating constructors)
   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
   #  if !defined(BOOST_CONTAINER_NO_CXX11_DELEGATING_CONSTRUCTORS)
      private:
      template<template<class ...> class Tuple, class... Args1, class... Args2, size_t... Indexes1, size_t... Indexes2>
      pair(Tuple<Args1...>& t1, Tuple<Args2...>& t2, index_tuple<Indexes1...>, index_tuple<Indexes2...>)
         : first (::boost::forward<Args1>(get<Indexes1>(t1))...)
         , second(::boost::forward<Args2>(get<Indexes2>(t2))...)
      {  (void) t1; (void)t2; }

      public:
      template< template<class ...> class Tuple, class... Args1, class... Args2
              , class = typename pair_impl::disable_if_boost_tuple< Tuple<Args1...> >::type>
      pair(piecewise_construct_t, Tuple<Args1...> t1, Tuple<Args2...> t2)
         : pair(t1, t2, typename build_number_seq<sizeof...(Args1)>::type(), typename build_number_seq<sizeof...(Args2)>::type())
      {
         BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
      }
   #  else
      //piecewise construction from variadic tuple (suboptimal, without delegating constructors)
      private:
      template<typename T, template<class ...> class Tuple, typename... Args>
      static T build_from_args(Tuple<Args...>&& t)
      {  return do_build_from_args<T>(::boost::move(t), typename build_number_seq<sizeof...(Args)>::type());   }

      template<typename T, template<class ...> class Tuple, typename... Args, std::size_t... Indexes>
      static T do_build_from_args(Tuple<Args...> && t, const index_tuple<Indexes...>&)
      {  (void)t; return T(::boost::forward<Args>(get<Indexes>(t))...);  }

      public:
      template< template<class ...> class Tuple, class... Args1, class... Args2
              , class = typename pair_impl::disable_if_boost_tuple< Tuple<Args1...> >::type>
      pair(piecewise_construct_t, Tuple<Args1...> t1, Tuple<Args2...> t2)
         : first  (build_from_args<first_type> (::boost::move(t1)))
         , second (build_from_args<second_type>(::boost::move(t2)))
      {
         BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));
      }
   #  endif   //BOOST_NO_CXX11_VARIADIC_TEMPLATES
   #elif defined(BOOST_MSVC) && (_CPPLIB_VER == 520)
      //MSVC 2010 tuple implementation
      #define BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE(N,M)\
      template< template<class, class, class, class, class, class, class, class, class, class> class StdTuple \
               BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
      pair( piecewise_construct_t\
          , StdTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,N),::std::tr1::_Nil)> p\
          , StdTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,M),::std::tr1::_Nil)> q)\
         : first(BOOST_MOVE_GET_IDX##N), second(BOOST_MOVE_GET_IDXQ##M)\
      { (void)p; (void)q;\
         BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));\
      }\
      //
      BOOST_MOVE_ITER2D_0TOMAX(9, BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE)
      #undef BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE
   #elif defined(BOOST_MSVC) && (_CPPLIB_VER == 540)
      #if _VARIADIC_MAX >= 9
      #define BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT 9
      #else
      #define BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT BOOST_MOVE_ADD(_VARIADIC_MAX, 1)
      #endif

      //MSVC 2012 tuple implementation
      #define BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_CODE(N,M)\
      template< template<BOOST_MOVE_REPEAT(_VARIADIC_MAX, class), class, class, class> class StdTuple \
               BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
      pair( piecewise_construct_t\
          , StdTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(BOOST_MOVE_ADD(_VARIADIC_MAX, 3),N),::std::_Nil) > p\
          , StdTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(BOOST_MOVE_ADD(_VARIADIC_MAX, 3),M),::std::_Nil) > q)\
         : first(BOOST_MOVE_GET_IDX##N), second(BOOST_MOVE_GET_IDXQ##M)\
      { (void)p; (void)q;\
         BOOST_STATIC_ASSERT((sizeof(std::pair<T1, T2>) == sizeof(pair<T1, T2>)));\
      }\
      //
      BOOST_MOVE_ITER2D_0TOMAX(BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT, BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_CODE)
      #undef BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE
      #undef BOOST_PAIR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT
   #endif

   //pair copy assignment
   pair& operator=(BOOST_COPY_ASSIGN_REF(pair) p)
   {
      first  = p.first;
      second = p.second;
      return *this;
   }

   //pair move assignment
   pair& operator=(BOOST_RV_REF(pair) p)
   {
      first  = ::boost::move(p.first);
      second = ::boost::move(p.second);
      return *this;
   }

   template <class D, class S>
   typename ::boost::container::dtl::disable_if_or
      < pair &
      , ::boost::container::dtl::is_same<T1, D>
      , ::boost::container::dtl::is_same<T2, S>
      >::type
      operator=(const pair<D, S>&p)
   {
      first  = p.first;
      second = p.second;
      return *this;
   }

   template <class D, class S>
   typename ::boost::container::dtl::disable_if_or
      < pair &
      , ::boost::container::dtl::is_same<T1, D>
      , ::boost::container::dtl::is_same<T2, S>
      >::type
      operator=(BOOST_RV_REF_BEG pair<D, S> BOOST_RV_REF_END p)
   {
      first  = ::boost::move(p.first);
      second = ::boost::move(p.second);
      return *this;
   }
//std::pair copy assignment
   pair& operator=(const std::pair<T1, T2> &p)
   {
      first  = p.first;
      second = p.second;
      return *this;
   }

   template <class D, class S>
   pair& operator=(const std::pair<D, S> &p)
   {
      first  = ::boost::move(p.first);
      second = ::boost::move(p.second);
      return *this;
   }

   //std::pair move assignment
   pair& operator=(BOOST_RV_REF_BEG std::pair<T1, T2> BOOST_RV_REF_END p)
   {
      first  = ::boost::move(p.first);
      second = ::boost::move(p.second);
      return *this;
   }

   template <class D, class S>
   pair& operator=(BOOST_RV_REF_BEG std::pair<D, S> BOOST_RV_REF_END p)
   {
      first  = ::boost::move(p.first);
      second = ::boost::move(p.second);
      return *this;
   }

   //swap
   void swap(pair& p)
   {
      ::boost::adl_move_swap(this->first, p.first);
      ::boost::adl_move_swap(this->second, p.second);
   }
};

template <class T1, class T2>
inline bool operator==(const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return static_cast<bool>(x.first == y.first && x.second == y.second);  }

template <class T1, class T2>
inline bool operator< (const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return static_cast<bool>(x.first < y.first ||
                         (!(y.first < x.first) && x.second < y.second)); }

template <class T1, class T2>
inline bool operator!=(const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return static_cast<bool>(!(x == y));  }

template <class T1, class T2>
inline bool operator> (const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return y < x;  }

template <class T1, class T2>
inline bool operator>=(const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return static_cast<bool>(!(x < y)); }

template <class T1, class T2>
inline bool operator<=(const pair<T1,T2>& x, const pair<T1,T2>& y)
{  return static_cast<bool>(!(y < x)); }

template <class T1, class T2>
inline pair<T1, T2> make_pair(T1 x, T2 y)
{  return pair<T1, T2>(x, y); }

template <class T1, class T2>
inline void swap(pair<T1, T2>& x, pair<T1, T2>& y)
{  x.swap(y);  }

}  //namespace dtl {
}  //namespace container {

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T1, class T2>
struct has_move_emulation_enabled< ::boost::container::dtl::pair<T1, T2> >
{
   static const bool value = true;
};

#endif

namespace move_detail{

template<class T>
struct is_class_or_union;

template <class T1, class T2>
struct is_class_or_union< ::boost::container::dtl::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = true;
};

template <class T1, class T2>
struct is_class_or_union< std::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = true;
};

template<class T>
struct is_union;

template <class T1, class T2>
struct is_union< ::boost::container::dtl::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = false;
};

template <class T1, class T2>
struct is_union< std::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = false;
};

template<class T>
struct is_class;

template <class T1, class T2>
struct is_class< ::boost::container::dtl::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = true;
};

template <class T1, class T2>
struct is_class< std::pair<T1, T2> >
//This specialization is needed to avoid instantiation of pair in
//is_class, and allow recursive maps.
{
   static const bool value = true;
};


//Triviality of pair
template<class T>
struct is_trivially_copy_constructible;

template<class A, class B>
struct is_trivially_copy_assignable
   <boost::container::dtl::pair<A,B> >
{
   static const bool value = boost::move_detail::is_trivially_copy_assignable<A>::value &&
                             boost::move_detail::is_trivially_copy_assignable<B>::value ;
};

template<class T>
struct is_trivially_move_constructible;

template<class A, class B>
struct is_trivially_move_assignable
   <boost::container::dtl::pair<A,B> >
{
   static const bool value = boost::move_detail::is_trivially_move_assignable<A>::value &&
                             boost::move_detail::is_trivially_move_assignable<B>::value ;
};

template<class T>
struct is_trivially_copy_assignable;

template<class A, class B>
struct is_trivially_copy_constructible<boost::container::dtl::pair<A,B> >
{
   static const bool value = boost::move_detail::is_trivially_copy_constructible<A>::value &&
                             boost::move_detail::is_trivially_copy_constructible<B>::value ;
};

template<class T>
struct is_trivially_move_assignable;

template<class A, class B>
struct is_trivially_move_constructible<boost::container::dtl::pair<A,B> >
{
   static const bool value = boost::move_detail::is_trivially_move_constructible<A>::value &&
                             boost::move_detail::is_trivially_move_constructible<B>::value ;
};

template<class T>
struct is_trivially_destructible;

template<class A, class B>
struct is_trivially_destructible<boost::container::dtl::pair<A,B> >
{
   static const bool value = boost::move_detail::is_trivially_destructible<A>::value &&
                             boost::move_detail::is_trivially_destructible<B>::value ;
};


}  //namespace move_detail{

}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_DETAIL_PAIR_HPP
