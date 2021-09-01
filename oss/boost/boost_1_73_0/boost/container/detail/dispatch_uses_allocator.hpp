//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_HPP
#define BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_HPP

#if defined (_MSC_VER)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <boost/container/allocator_traits.hpp>
#include <boost/container/uses_allocator.hpp>

#include <boost/container/detail/addressof.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/pair.hpp>
#include <boost/container/detail/type_traits.hpp>

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif
#include <boost/move/utility_core.hpp>

#include <boost/core/no_exceptions_support.hpp>

namespace boost { namespace container {

namespace dtl {


// Check if we can detect is_convertible using advanced SFINAE expressions
#if !defined(BOOST_NO_CXX11_DECLTYPE) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   //! Code inspired by Mathias Gaunard's is_convertible.cpp found in the Boost mailing list
   //! http://boost.2283326.n4.nabble.com/type-traits-is-constructible-when-decltype-is-supported-td3575452.html
   //! Thanks Mathias!

   //With variadic templates, we need a single class to implement the trait
   template<class T, class ...Args>
   struct is_constructible
   {
      typedef char yes_type;
      struct no_type
      { char padding[2]; };

      template<std::size_t N>
      struct dummy;

      template<class X>
      static decltype(X(boost::move_detail::declval<Args>()...), true_type()) test(int);

      template<class X>
      static no_type test(...);

      static const bool value = sizeof(test<T>(0)) == sizeof(yes_type);
   };

   template <class T, class InnerAlloc, class ...Args>
   struct is_constructible_with_allocator_prefix
      : is_constructible<T, allocator_arg_t, InnerAlloc, Args...>
   {};

#else    // #if !defined(BOOST_NO_SFINAE_EXPR) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   //Without advanced SFINAE expressions, we can't use is_constructible
   //so backup to constructible_with_allocator_xxx

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template <class T, class InnerAlloc, class ...Args>
   struct is_constructible_with_allocator_prefix
      : constructible_with_allocator_prefix<T>
   {};

   template <class T, class InnerAlloc, class ...Args>
   struct is_constructible_with_allocator_suffix
      : constructible_with_allocator_suffix<T>
   {};

   #else    // #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template <class T, class InnerAlloc, BOOST_MOVE_CLASSDFLT9>
   struct is_constructible_with_allocator_prefix
      : constructible_with_allocator_prefix<T>
   {};

   template <class T, class InnerAlloc, BOOST_MOVE_CLASSDFLT9>
   struct is_constructible_with_allocator_suffix
      : constructible_with_allocator_suffix<T>
   {};

   #endif   // #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#endif   // #if !defined(BOOST_NO_SFINAE_EXPR)

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

template < typename ConstructAlloc
         , typename ArgAlloc
         , typename T
         , class ...Args
         >
inline typename dtl::enable_if_and
   < void
   , dtl::is_not_pair<T>
   , dtl::not_< uses_allocator<T, ArgAlloc> >
   >::type dispatch_uses_allocator
   ( ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p, BOOST_FWD_REF(Args)...args)
{
   (void)arg_alloc;
   allocator_traits<ConstructAlloc>::construct(construct_alloc, p, ::boost::forward<Args>(args)...);
}

// allocator_arg_t
template < typename ConstructAlloc
         , typename ArgAlloc
         , typename T
         , class ...Args
         >
inline typename dtl::enable_if_and
   < void
   , dtl::is_not_pair<T>
   , uses_allocator<T, ArgAlloc>
   , is_constructible_with_allocator_prefix<T, ArgAlloc, Args...>
   >::type dispatch_uses_allocator
   ( ConstructAlloc& construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p, BOOST_FWD_REF(Args) ...args)
{
   allocator_traits<ConstructAlloc>::construct
      ( construct_alloc, p, allocator_arg
      , ::boost::forward<ArgAlloc>(arg_alloc), ::boost::forward<Args>(args)...);
}

// allocator suffix
template < typename ConstructAlloc
         , typename ArgAlloc
         , typename T
         , class ...Args
         >
inline typename dtl::enable_if_and
   < void
   , dtl::is_not_pair<T>
   , uses_allocator<T, ArgAlloc>
   , dtl::not_<is_constructible_with_allocator_prefix<T, ArgAlloc, Args...> >
   >::type dispatch_uses_allocator
   ( ConstructAlloc& construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p, BOOST_FWD_REF(Args)...args)
{
   allocator_traits<ConstructAlloc>::construct
      (construct_alloc, p, ::boost::forward<Args>(args)..., ::boost::forward<ArgAlloc>(arg_alloc));
}

#else    //#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#define BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE(N) \
   template <typename ConstructAlloc, typename ArgAlloc, typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline typename dtl::enable_if_and\
      < void\
      , dtl::is_not_pair<T>\
      , dtl::not_<uses_allocator<T, ArgAlloc> >\
      >::type\
      dispatch_uses_allocator\
      (ConstructAlloc &construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      (void)arg_alloc;\
      allocator_traits<ConstructAlloc>::construct(construct_alloc, p BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
   }\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE)
#undef BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE

#define BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE(N) \
   template < typename ConstructAlloc, typename ArgAlloc, typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline typename dtl::enable_if_and\
      < void\
      , dtl::is_not_pair<T>\
      , uses_allocator<T, ArgAlloc>\
      , is_constructible_with_allocator_prefix<T, ArgAlloc BOOST_MOVE_I##N BOOST_MOVE_TARG##N>\
      >::type\
      dispatch_uses_allocator\
      (ConstructAlloc& construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      allocator_traits<ConstructAlloc>::construct\
         (construct_alloc, p, allocator_arg, ::boost::forward<ArgAlloc>(arg_alloc) BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
   }\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE)
#undef BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE

#define BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE(N) \
   template < typename ConstructAlloc, typename ArgAlloc, typename T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline typename dtl::enable_if_and\
      < void\
      , dtl::is_not_pair<T>\
      , uses_allocator<T, ArgAlloc>\
      , dtl::not_<is_constructible_with_allocator_prefix<T, ArgAlloc BOOST_MOVE_I##N BOOST_MOVE_TARG##N> >\
      >::type\
      dispatch_uses_allocator\
      (ConstructAlloc& construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, T* p BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      allocator_traits<ConstructAlloc>::construct\
         (construct_alloc, p BOOST_MOVE_I##N BOOST_MOVE_FWD##N, ::boost::forward<ArgAlloc>(arg_alloc));\
   }\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE)
#undef BOOST_CONTAINER_SCOPED_ALLOCATOR_DISPATCH_USES_ALLOCATOR_CODE

#endif   //#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

template < typename ConstructAlloc
         , typename ArgAlloc
         , typename Pair
         > inline 
BOOST_CONTAINER_DOC1ST(void, typename dtl::enable_if<dtl::is_pair<Pair> BOOST_MOVE_I void >::type)
   dispatch_uses_allocator
   ( ConstructAlloc & construct_alloc
   , BOOST_FWD_REF(ArgAlloc) arg_alloc
   , Pair* p)
{
   (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->first));
   BOOST_TRY{
      (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->second));
   }
   BOOST_CATCH(...) {
      allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(p->first));
      BOOST_RETHROW
   }
   BOOST_CATCH_END
}


template < typename ConstructAlloc
         , typename ArgAlloc
         , class Pair, class U, class V>
BOOST_CONTAINER_DOC1ST(void, typename dtl::enable_if<dtl::is_pair<Pair> BOOST_MOVE_I void>::type)
   dispatch_uses_allocator
   ( ConstructAlloc & construct_alloc
   , BOOST_FWD_REF(ArgAlloc) arg_alloc
   , Pair* p, BOOST_FWD_REF(U) x, BOOST_FWD_REF(V) y)
{
   (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->first), ::boost::forward<U>(x));
   BOOST_TRY{
      (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->second), ::boost::forward<V>(y));
   }
   BOOST_CATCH(...){
      allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(p->first));
      BOOST_RETHROW
   }
   BOOST_CATCH_END
}

template < typename ConstructAlloc
         , typename ArgAlloc
         , class Pair, class Pair2>
BOOST_CONTAINER_DOC1ST(void, typename dtl::enable_if< dtl::is_pair<Pair> BOOST_MOVE_I void >::type)
   dispatch_uses_allocator
   (ConstructAlloc & construct_alloc
   , BOOST_FWD_REF(ArgAlloc) arg_alloc
   , Pair* p, Pair2& x)
{  (dispatch_uses_allocator)(construct_alloc, arg_alloc, p, x.first, x.second);  }

template < typename ConstructAlloc
         , typename ArgAlloc
         , class Pair, class Pair2>
typename dtl::enable_if_and
   < void
   , dtl::is_pair<Pair>
   , dtl::not_<boost::move_detail::is_reference<Pair2> > >::type //This is needed for MSVC10 and ambiguous overloads
   dispatch_uses_allocator
   (ConstructAlloc & construct_alloc
      , BOOST_FWD_REF(ArgAlloc) arg_alloc
      , Pair* p, BOOST_RV_REF_BEG Pair2 BOOST_RV_REF_END x)
{  (dispatch_uses_allocator)(construct_alloc, arg_alloc, p, ::boost::move(x.first), ::boost::move(x.second));  }


//piecewise construction from boost::tuple
#define BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE(N,M)\
template< typename ConstructAlloc, typename ArgAlloc, class Pair \
        , template<class, class, class, class, class, class, class, class, class, class> class BoostTuple \
         BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
typename dtl::enable_if< dtl::is_pair<Pair> BOOST_MOVE_I void>::type\
   dispatch_uses_allocator( ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* pair, piecewise_construct_t\
      , BoostTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,N),::boost::tuples::null_type)> p\
      , BoostTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,M),::boost::tuples::null_type)> q)\
{\
   (void)p; (void)q;\
   (dispatch_uses_allocator)\
      (construct_alloc, arg_alloc, dtl::addressof(pair->first) BOOST_MOVE_I_IF(N) BOOST_MOVE_TMPL_GET##N);\
   BOOST_TRY{\
      (dispatch_uses_allocator)\
         (construct_alloc, arg_alloc, dtl::addressof(pair->second) BOOST_MOVE_I_IF(M) BOOST_MOVE_TMPL_GETQ##M);\
   }\
   BOOST_CATCH(...) {\
      allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(pair->first));\
      BOOST_RETHROW\
   }\
   BOOST_CATCH_END\
}\
//
BOOST_MOVE_ITER2D_0TOMAX(9, BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE)
#undef BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_BOOST_TUPLE_CODE

//piecewise construction from Std Tuple
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template< typename ConstructAlloc, typename ArgAlloc, class Pair
           , template<class ...> class Tuple, class... Args1, class... Args2, size_t... Indexes1, size_t... Indexes2>
   void dispatch_uses_allocator_index( ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* pair
                                    , Tuple<Args1...>& t1, Tuple<Args2...>& t2, index_tuple<Indexes1...>, index_tuple<Indexes2...>)
   {
      (void)t1; (void)t2;
      (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(pair->first), ::boost::forward<Args1>(get<Indexes1>(t1))...);
      BOOST_TRY{
         (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(pair->second), ::boost::forward<Args2>(get<Indexes2>(t2))...);
      }
      BOOST_CATCH(...){
         allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(pair->first));
         BOOST_RETHROW
      }
      BOOST_CATCH_END
   }

   template< typename ConstructAlloc, typename ArgAlloc, class Pair
           , template<class ...> class Tuple, class... Args1, class... Args2>
   typename dtl::enable_if< dtl::is_pair<Pair>, void >::type
      dispatch_uses_allocator( ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* pair, piecewise_construct_t
                             , Tuple<Args1...> t1, Tuple<Args2...> t2)
   {
      (dispatch_uses_allocator_index)( construct_alloc, arg_alloc, pair, t1, t2
                                     , typename build_number_seq<sizeof...(Args1)>::type()
                                     , typename build_number_seq<sizeof...(Args2)>::type());
   }

#elif defined(BOOST_MSVC) && (_CPPLIB_VER == 520)

   //MSVC 2010 tuple implementation
   #define BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE(N,M)\
   template< typename ConstructAlloc, typename ArgAlloc, class Pair\
           , template<class, class, class, class, class, class, class, class, class, class> class StdTuple\
            BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
   typename dtl::enable_if< dtl::is_pair<Pair> BOOST_MOVE_I void>::type\
      dispatch_uses_allocator(ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* pair, piecewise_construct_t\
                           , StdTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,N),::std::tr1::_Nil)> p\
                           , StdTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(10,M),::std::tr1::_Nil)> q)\
   {\
      (void)p; (void)q;\
      (dispatch_uses_allocator)\
         (construct_alloc, arg_alloc, dtl::addressof(pair->first) BOOST_MOVE_I_IF(N) BOOST_MOVE_GET_IDX##N);\
      BOOST_TRY{\
         (dispatch_uses_allocator)\
            (construct_alloc, arg_alloc, dtl::addressof(pair->second) BOOST_MOVE_I_IF(M) BOOST_MOVE_GET_IDXQ##M);\
      }\
      BOOST_CATCH(...) {\
         allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(pair->first));\
         BOOST_RETHROW\
      }\
      BOOST_CATCH_END\
   }\
   //
   BOOST_MOVE_ITER2D_0TOMAX(9, BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE)
   #undef BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE

#elif defined(BOOST_MSVC) && (_CPPLIB_VER == 540)
   #if _VARIADIC_MAX >= 9
   #define BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT 9
   #else
   #define BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT BOOST_MOVE_ADD(_VARIADIC_MAX, 1)
   #endif

   //MSVC 2012 tuple implementation
   #define BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_CODE(N,M)\
   template< typename ConstructAlloc, typename ArgAlloc, class Pair\
            , template<BOOST_MOVE_REPEAT(_VARIADIC_MAX, class), class, class, class> class StdTuple \
            BOOST_MOVE_I_IF(BOOST_MOVE_OR(N,M)) BOOST_MOVE_CLASS##N BOOST_MOVE_I_IF(BOOST_MOVE_AND(N,M)) BOOST_MOVE_CLASSQ##M > \
   typename dtl::enable_if< dtl::is_pair<Pair> BOOST_MOVE_I void>::type\
      dispatch_uses_allocator\
         ( ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* pair, piecewise_construct_t\
         , StdTuple<BOOST_MOVE_TARG##N  BOOST_MOVE_I##N BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(BOOST_MOVE_ADD(_VARIADIC_MAX, 3),N),::std::_Nil) > p\
         , StdTuple<BOOST_MOVE_TARGQ##M BOOST_MOVE_I##M BOOST_MOVE_REPEAT(BOOST_MOVE_SUB(BOOST_MOVE_ADD(_VARIADIC_MAX, 3),M),::std::_Nil) > q)\
   {\
      (void)p; (void)q;\
      (dispatch_uses_allocator)\
         (construct_alloc, arg_alloc, dtl::addressof(pair->first) BOOST_MOVE_I_IF(N) BOOST_MOVE_GET_IDX##N);\
      BOOST_TRY{\
         (dispatch_uses_allocator)\
            (construct_alloc, arg_alloc, dtl::addressof(pair->second) BOOST_MOVE_I_IF(M) BOOST_MOVE_GET_IDXQ##M);\
      }\
      BOOST_CATCH(...) {\
         allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(pair->first));\
         BOOST_RETHROW\
      }\
      BOOST_CATCH_END\
   }\
   //
   BOOST_MOVE_ITER2D_0TOMAX(BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT, BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_CODE)
   #undef BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2010_TUPLE_CODE
   #undef BOOST_DISPATCH_USES_ALLOCATOR_PIECEWISE_CONSTRUCT_MSVC2012_TUPLE_MAX_IT

#endif   //!defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

template < typename ConstructAlloc
         , typename ArgAlloc
         , class Pair, class KeyType, class ... Args>
typename dtl::enable_if< dtl::is_pair<Pair>, void >::type
   dispatch_uses_allocator
   (ConstructAlloc & construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* p, try_emplace_t, BOOST_FWD_REF(KeyType) k, BOOST_FWD_REF(Args) ...args)
{
   (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->first), ::boost::forward<KeyType>(k));
   BOOST_TRY{
      (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->second), ::boost::forward<Args>(args)...);
   }
   BOOST_CATCH(...) {
      allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(p->first));
      BOOST_RETHROW
   }
   BOOST_CATCH_END
}

#else

#define BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_PAIR_TRY_EMPLACE_CODE(N) \
   template <typename ConstructAlloc, typename ArgAlloc, class Pair, class KeyType BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline typename dtl::enable_if\
      < dtl::is_pair<Pair> BOOST_MOVE_I void >::type\
      dispatch_uses_allocator\
      (ConstructAlloc &construct_alloc, BOOST_FWD_REF(ArgAlloc) arg_alloc, Pair* p, try_emplace_t, \
       BOOST_FWD_REF(KeyType) k BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->first), ::boost::forward<KeyType>(k));\
      BOOST_TRY{\
         (dispatch_uses_allocator)(construct_alloc, arg_alloc, dtl::addressof(p->second) BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      }\
      BOOST_CATCH(...) {\
         allocator_traits<ConstructAlloc>::destroy(construct_alloc, dtl::addressof(p->first));\
         BOOST_RETHROW\
      }\
      BOOST_CATCH_END\
   }\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_PAIR_TRY_EMPLACE_CODE)
#undef BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_PAIR_TRY_EMPLACE_CODE

#endif   //!defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

}  //namespace dtl

}} // namespace boost { namespace container {

#include <boost/container/detail/config_end.hpp>

#endif //  BOOST_CONTAINER_DISPATCH_USES_ALLOCATOR_HPP
