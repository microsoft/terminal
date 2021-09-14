//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_MOVE_DEFAULT_DELETE_HPP_INCLUDED
#define BOOST_MOVE_DEFAULT_DELETE_HPP_INCLUDED

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/workaround.hpp>
#include <boost/move/detail/unique_ptr_meta_utils.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/static_assert.hpp>

#include <cstddef>   //For std::size_t,std::nullptr_t

//!\file
//! Describes the default deleter (destruction policy) of <tt>unique_ptr</tt>: <tt>default_delete</tt>.

namespace boost{
// @cond
namespace move_upd {

namespace bmupmu = ::boost::move_upmu;

////////////////////////////////////////
////        enable_def_del
////////////////////////////////////////

//compatible with a pointer type T*:
//When either Y* is convertible to T*
//Y is U[N] and T is U cv []
template<class U, class T>
struct def_del_compatible_cond
   : bmupmu::is_convertible<U*, T*>
{};

template<class U, class T, std::size_t N>
struct def_del_compatible_cond<U[N], T[]>
   : def_del_compatible_cond<U[], T[]>
{};

template<class U, class T, class Type = bmupmu::nat>
struct enable_def_del
   : bmupmu::enable_if_c<def_del_compatible_cond<U, T>::value, Type>
{};

////////////////////////////////////////
////        enable_defdel_call
////////////////////////////////////////

//When 2nd is T[N], 1st(*)[N] shall be convertible to T(*)[N]; 
//When 2nd is T[],  1st(*)[] shall be convertible to T(*)[]; 
//Otherwise, 1st* shall be convertible to 2nd*.

template<class U, class T, class Type = bmupmu::nat>
struct enable_defdel_call
   : public enable_def_del<U, T, Type>
{};

template<class U, class T, class Type>
struct enable_defdel_call<U, T[], Type>
   : public enable_def_del<U[], T[], Type>
{};

template<class U, class T, class Type, std::size_t N>
struct enable_defdel_call<U, T[N], Type>
   : public enable_def_del<U[N], T[N], Type>
{};

////////////////////////////////////////
////     Some bool literal zero conversion utilities
////////////////////////////////////////

struct bool_conversion {int for_bool; int for_arg(); };
typedef int bool_conversion::* explicit_bool_arg;

#if !defined(BOOST_NO_CXX11_NULLPTR) && !defined(BOOST_NO_CXX11_DECLTYPE)
   typedef decltype(nullptr) nullptr_type;
#elif !defined(BOOST_NO_CXX11_NULLPTR)
   typedef std::nullptr_t nullptr_type;
#else
   typedef int (bool_conversion::*nullptr_type)();
#endif

template<bool B>
struct is_array_del
{};

template<class T>
void call_delete(T *p, is_array_del<true>)
{
   delete [] p;
}

template<class T>
void call_delete(T *p, is_array_del<false>)
{
   delete p;
}

}  //namespace move_upd {
// @endcond

namespace movelib {

namespace bmupd = boost::move_upd;
namespace bmupmu = ::boost::move_upmu;

//!The class template <tt>default_delete</tt> serves as the default deleter
//!(destruction policy) for the class template <tt>unique_ptr</tt>.
//!
//! \tparam T The type to be deleted. It may be an incomplete type
template <class T>
struct default_delete
{
   //! Default constructor.
   //!
   BOOST_CONSTEXPR default_delete()
   //Avoid "defaulted on its first declaration must not have an exception-specification" error for GCC 4.6
   #if !defined(BOOST_GCC) || (BOOST_GCC < 40600 && BOOST_GCC >= 40700) || defined(BOOST_MOVE_DOXYGEN_INVOKED)
   BOOST_NOEXCEPT
   #endif
   #if !defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS) || defined(BOOST_MOVE_DOXYGEN_INVOKED)
   = default;
   #else
   {};
   #endif

   #if defined(BOOST_MOVE_DOXYGEN_INVOKED)
   //! Trivial copy constructor
   //!
   default_delete(const default_delete&) BOOST_NOEXCEPT = default;
   //! Trivial assignment
   //!
   default_delete &operator=(const default_delete&) BOOST_NOEXCEPT = default;
   #else
   typedef typename bmupmu::remove_extent<T>::type element_type;
   #endif

   //! <b>Effects</b>: Constructs a default_delete object from another <tt>default_delete<U></tt> object.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!   - If T is not an array type and U* is implicitly convertible to T*.
   //!   - If T is an array type and U* is a more CV qualified pointer to remove_extent<T>::type.
   template <class U>
   default_delete(const default_delete<U>&
      BOOST_MOVE_DOCIGN(BOOST_MOVE_I typename bmupd::enable_def_del<U BOOST_MOVE_I T>::type* =0)
      ) BOOST_NOEXCEPT
   {
      //If T is not an array type, U derives from T
      //and T has no virtual destructor, then you have a problem
      BOOST_STATIC_ASSERT(( !::boost::move_upmu::missing_virtual_destructor<default_delete, U>::value ));
   }

   //! <b>Effects</b>: Constructs a default_delete object from another <tt>default_delete<U></tt> object.
   //!
   //! <b>Remarks</b>: This constructor shall not participate in overload resolution unless:
   //!   - If T is not an array type and U* is implicitly convertible to T*.
   //!   - If T is an array type and U* is a more CV qualified pointer to remove_extent<T>::type.
   template <class U>
   BOOST_MOVE_DOC1ST(default_delete&, 
      typename bmupd::enable_def_del<U BOOST_MOVE_I T BOOST_MOVE_I default_delete &>::type)
      operator=(const default_delete<U>&) BOOST_NOEXCEPT
   {
      //If T is not an array type, U derives from T
      //and T has no virtual destructor, then you have a problem
      BOOST_STATIC_ASSERT(( !::boost::move_upmu::missing_virtual_destructor<default_delete, U>::value ));
      return *this;
   }

   //! <b>Effects</b>: if T is not an array type, calls <tt>delete</tt> on static_cast<T*>(ptr),
   //!   otherwise calls <tt>delete[]</tt> on static_cast<remove_extent<T>::type*>(ptr).
   //!
   //! <b>Remarks</b>: If U is an incomplete type, the program is ill-formed.
   //!   This operator shall not participate in overload resolution unless:
   //!      - T is not an array type and U* is convertible to T*, OR
   //!      - T is an array type, and remove_cv<U>::type is the same type as
   //!         remove_cv<remove_extent<T>::type>::type and U* is convertible to remove_extent<T>::type*.
   template <class U>
   BOOST_MOVE_DOC1ST(void, typename bmupd::enable_defdel_call<U BOOST_MOVE_I T BOOST_MOVE_I void>::type)
      operator()(U* ptr) const BOOST_NOEXCEPT
   {
      //U must be a complete type
      BOOST_STATIC_ASSERT(sizeof(U) > 0);
      //If T is not an array type, U derives from T
      //and T has no virtual destructor, then you have a problem
      BOOST_STATIC_ASSERT(( !::boost::move_upmu::missing_virtual_destructor<default_delete, U>::value ));
      element_type * const p = static_cast<element_type*>(ptr);
      move_upd::call_delete(p, move_upd::is_array_del<bmupmu::is_array<T>::value>());
   }

   //! <b>Effects</b>: Same as <tt>(*this)(static_cast<element_type*>(nullptr))</tt>.
   //!
   void operator()(BOOST_MOVE_DOC0PTR(bmupd::nullptr_type)) const BOOST_NOEXCEPT
   {  BOOST_STATIC_ASSERT(sizeof(element_type) > 0);  }
};

}  //namespace movelib {
}  //namespace boost{

#include <boost/move/detail/config_end.hpp>

#endif   //#ifndef BOOST_MOVE_DEFAULT_DELETE_HPP_INCLUDED
