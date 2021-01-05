//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2012-2012.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_UNIQUE_PTR_DETAIL_META_UTILS_HPP
#define BOOST_MOVE_UNIQUE_PTR_DETAIL_META_UTILS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <cstddef>   //for std::size_t

//Small meta-typetraits to support move

namespace boost {

namespace movelib {

template <class T>
struct default_delete;

}  //namespace movelib {

#ifdef BOOST_NO_CXX11_RVALUE_REFERENCES
//Forward declare boost::rv
template <class T> class rv;
#endif

namespace move_upmu {

//////////////////////////////////////
//              nat
//////////////////////////////////////
struct nat{};

//////////////////////////////////////
//            natify
//////////////////////////////////////
template <class T> struct natify{};

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

//enable_if_
template <bool B, class T = nat>
struct enable_if_c
{
   typedef T type;
};

//////////////////////////////////////
//          enable_if_c
//////////////////////////////////////
template <class T>
struct enable_if_c<false, T> {};

//////////////////////////////////////
//           enable_if
//////////////////////////////////////
template <class Cond, class T = nat>
struct enable_if : public enable_if_c<Cond::value, T> {};

//////////////////////////////////////
//          remove_reference
//////////////////////////////////////
template<class T>
struct remove_reference
{
   typedef T type;
};

template<class T>
struct remove_reference<T&>
{
   typedef T type;
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template<class T>
struct remove_reference<T&&>
{
   typedef T type;
};

#else

template<class T>
struct remove_reference< rv<T> >
{
   typedef T type;
};

template<class T>
struct remove_reference< rv<T> &>
{
   typedef T type;
};

template<class T>
struct remove_reference< const rv<T> &>
{
   typedef T type;
};


#endif

//////////////////////////////////////
//             remove_const
//////////////////////////////////////
template< class T >
struct remove_const
{
   typedef T type;
};

template< class T >
struct remove_const<const T>
{
   typedef T type;
};

//////////////////////////////////////
//             remove_volatile
//////////////////////////////////////
template< class T >
struct remove_volatile
{
   typedef T type;
};

template< class T >
struct remove_volatile<volatile T>
{
   typedef T type;
};

//////////////////////////////////////
//             remove_cv
//////////////////////////////////////
template< class T >
struct remove_cv
{
    typedef typename remove_volatile
      <typename remove_const<T>::type>::type type;
};

//////////////////////////////////////
//          remove_extent
//////////////////////////////////////
template<class T>
struct remove_extent
{
   typedef T type;
};
 
template<class T>
struct remove_extent<T[]>
{
   typedef T type;
};
 
template<class T, std::size_t N>
struct remove_extent<T[N]>
{
   typedef T type;
};

//////////////////////////////////////
//             extent
//////////////////////////////////////

template<class T, unsigned N = 0>
struct extent
{
   static const std::size_t value = 0;
};
 
template<class T>
struct extent<T[], 0> 
{
   static const std::size_t value = 0;
};

template<class T, unsigned N>
struct extent<T[], N>
{
   static const std::size_t value = extent<T, N-1>::value;
};

template<class T, std::size_t N>
struct extent<T[N], 0> 
{
   static const std::size_t value = N;
};
 
template<class T, std::size_t I, unsigned N>
struct extent<T[I], N>
{
   static const std::size_t value = extent<T, N-1>::value;
};

//////////////////////////////////////
//      add_lvalue_reference
//////////////////////////////////////
template<class T>
struct add_lvalue_reference
{
   typedef T& type;
};

template<class T>
struct add_lvalue_reference<T&>
{
   typedef T& type;
};

template<>
struct add_lvalue_reference<void>
{
   typedef void type;
};

template<>
struct add_lvalue_reference<const void>
{
   typedef const void type;
};

template<>
struct add_lvalue_reference<volatile void>
{
   typedef volatile void type;
};

template<>
struct add_lvalue_reference<const volatile void>
{
   typedef const volatile void type;
};

template<class T>
struct add_const_lvalue_reference
{
   typedef typename remove_reference<T>::type   t_unreferenced;
   typedef const t_unreferenced                 t_unreferenced_const;
   typedef typename add_lvalue_reference
      <t_unreferenced_const>::type              type;
};

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
//             is_pointer
//////////////////////////////////////
template< class T >
struct is_pointer
{
    static const bool value = false;
};

template< class T >
struct is_pointer<T*>
{
    static const bool value = true;
};

//////////////////////////////////////
//             is_reference
//////////////////////////////////////
template< class T >
struct is_reference
{
    static const bool value = false;
};

template< class T >
struct is_reference<T&>
{
    static const bool value = true;
};

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

template< class T >
struct is_reference<T&&>
{
    static const bool value = true;
};

#endif

//////////////////////////////////////
//             is_lvalue_reference
//////////////////////////////////////
template<class T>
struct is_lvalue_reference
{
    static const bool value = false;
};

template<class T>
struct is_lvalue_reference<T&>
{
    static const bool value = true;
};

//////////////////////////////////////
//          is_array
//////////////////////////////////////
template<class T>
struct is_array
{
   static const bool value = false;
};
 
template<class T>
struct is_array<T[]>
{
   static const bool value = true;
};
 
template<class T, std::size_t N>
struct is_array<T[N]>
{
   static const bool value = true;
};

//////////////////////////////////////
//          has_pointer_type
//////////////////////////////////////
template <class T>
struct has_pointer_type
{
   struct two { char c[2]; };
   template <class U> static two test(...);
   template <class U> static char test(typename U::pointer* = 0);
   static const bool value = sizeof(test<T>(0)) == 1;
};

//////////////////////////////////////
//             pointer_type
//////////////////////////////////////
template <class T, class D, bool = has_pointer_type<D>::value>
struct pointer_type_imp
{
    typedef typename D::pointer type;
};

template <class T, class D>
struct pointer_type_imp<T, D, false>
{
    typedef T* type;
};

template <class T, class D>
struct pointer_type
{
    typedef typename pointer_type_imp
      <typename remove_extent<T>::type, typename remove_reference<D>::type>::type type;
};

//////////////////////////////////////
//           is_convertible
//////////////////////////////////////
#if defined(_MSC_VER) && (_MSC_VER >= 1400)

//use intrinsic since in MSVC
//overaligned types can't go through ellipsis
template <class T, class U>
struct is_convertible
{
   static const bool value = __is_convertible_to(T, U);
};

#else

template <class T, class U>
class is_convertible
{
   typedef typename add_lvalue_reference<T>::type t_reference;
   typedef char true_t;
   class false_t { char dummy[2]; };
   static false_t dispatch(...);
   static true_t  dispatch(U);
   static t_reference       trigger();
   public:
   static const bool value = sizeof(dispatch(trigger())) == sizeof(true_t);
};

#endif

//////////////////////////////////////
//       is_unary_function
//////////////////////////////////////
#if defined(BOOST_MSVC) || defined(__BORLANDC_)
#define BOOST_MOVE_TT_DECL __cdecl
#else
#define BOOST_MOVE_TT_DECL
#endif

#if defined(_MSC_EXTENSIONS) && !defined(__BORLAND__) && !defined(_WIN64) && !defined(_M_ARM) && !defined(_M_ARM64) && !defined(UNDER_CE)
#define BOOST_MOVE_TT_TEST_MSC_FUNC_SIGS
#endif

template <typename T>
struct is_unary_function_impl
{  static const bool value = false; };

// avoid duplicate definitions of is_unary_function_impl
#ifndef BOOST_MOVE_TT_TEST_MSC_FUNC_SIGS

template <typename R>
struct is_unary_function_impl<R (*)()>
{  static const bool value = true;  };

template <typename R>
struct is_unary_function_impl<R (*)(...)>
{  static const bool value = true;  };

#else // BOOST_MOVE_TT_TEST_MSC_FUNC_SIGS

template <typename R>
struct is_unary_function_impl<R (__stdcall*)()>
{  static const bool value = true;  };

#ifndef _MANAGED

template <typename R>
struct is_unary_function_impl<R (__fastcall*)()>
{  static const bool value = true;  };

#endif

template <typename R>
struct is_unary_function_impl<R (__cdecl*)()>
{  static const bool value = true;  };

template <typename R>
struct is_unary_function_impl<R (__cdecl*)(...)>
{  static const bool value = true;  };

#endif

// avoid duplicate definitions of is_unary_function_impl
#ifndef BOOST_MOVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0>
struct is_unary_function_impl<R (*)(T0)>
{  static const bool value = true;  };

template <typename R, class T0>
struct is_unary_function_impl<R (*)(T0...)>
{  static const bool value = true;  };

#else // BOOST_MOVE_TT_TEST_MSC_FUNC_SIGS

template <typename R, class T0>
struct is_unary_function_impl<R (__stdcall*)(T0)>
{  static const bool value = true;  };

#ifndef _MANAGED

template <typename R, class T0>
struct is_unary_function_impl<R (__fastcall*)(T0)>
{  static const bool value = true;  };

#endif

template <typename R, class T0>
struct is_unary_function_impl<R (__cdecl*)(T0)>
{  static const bool value = true;  };

template <typename R, class T0>
struct is_unary_function_impl<R (__cdecl*)(T0...)>
{  static const bool value = true;  };

#endif

template <typename T>
struct is_unary_function_impl<T&>
{  static const bool value = false; };

template<typename T>
struct is_unary_function
{  static const bool value = is_unary_function_impl<T>::value;   };

//////////////////////////////////////
//       has_virtual_destructor
//////////////////////////////////////
#if (defined(BOOST_MSVC) && defined(BOOST_MSVC_FULL_VER) && (BOOST_MSVC_FULL_VER >=140050215))\
         || (defined(BOOST_INTEL) && defined(_MSC_VER) && (_MSC_VER >= 1500))
#  define BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T) __has_virtual_destructor(T)
#elif defined(BOOST_CLANG) && defined(__has_feature)
#  if __has_feature(has_virtual_destructor)
#     define BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T) __has_virtual_destructor(T)
#  endif
#elif defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3) && !defined(__GCCXML__))) && !defined(BOOST_CLANG)
#  define BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T) __has_virtual_destructor(T)
#elif defined(__ghs__) && (__GHS_VERSION_NUMBER >= 600)
#  define BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T) __has_virtual_destructor(T)
#elif defined(__CODEGEARC__)
#  define BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T) __has_virtual_destructor(T)
#endif

#ifdef BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR
   template<class T>
   struct has_virtual_destructor{   static const bool value = BOOST_MOVEUP_HAS_VIRTUAL_DESTRUCTOR(T);  };
#else
   //If no intrinsic is available you trust the programmer knows what is doing
   template<class T>
   struct has_virtual_destructor{   static const bool value = true;  };
#endif

//////////////////////////////////////
//       missing_virtual_destructor
//////////////////////////////////////

template< class T, class U
        , bool enable =  is_convertible< U*, T*>::value &&
                        !is_array<T>::value &&
                        !is_same<typename remove_cv<T>::type, void>::value &&
                        !is_same<typename remove_cv<U>::type, typename remove_cv<T>::type>::value
        >
struct missing_virtual_destructor_default_delete
{  static const bool value = !has_virtual_destructor<T>::value;  };

template<class T, class U>
struct missing_virtual_destructor_default_delete<T, U, false>
{  static const bool value = false;  };

template<class Deleter, class U>
struct missing_virtual_destructor
{  static const bool value = false;  };

template<class T, class U>
struct missing_virtual_destructor< ::boost::movelib::default_delete<T>, U >
   : missing_virtual_destructor_default_delete<T, U>
{};

}  //namespace move_upmu {
}  //namespace boost {

#endif //#ifndef BOOST_MOVE_UNIQUE_PTR_DETAIL_META_UTILS_HPP
