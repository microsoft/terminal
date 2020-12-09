//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_MOVE_MAKE_UNIQUE_HPP_INCLUDED
#define BOOST_MOVE_MAKE_UNIQUE_HPP_INCLUDED

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/workaround.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/move/unique_ptr.hpp>
#include <cstddef>   //for std::size_t
#include <boost/move/detail/unique_ptr_meta_utils.hpp>
#ifdef BOOST_NO_CXX11_VARIADIC_TEMPLATES
#  include <boost/move/detail/fwd_macros.hpp>
#endif

//!\file
//! Defines "make_unique" functions, which are factories to create instances
//! of unique_ptr depending on the passed arguments.
//!
//! This header can be a bit heavyweight in C++03 compilers due to the use of the
//! preprocessor library, that's why it's a a separate header from <tt>unique_ptr.hpp</tt>
 
#if !defined(BOOST_MOVE_DOXYGEN_INVOKED)

namespace std {   //no namespace versioning in clang+libc++

struct nothrow_t;

}  //namespace std {

namespace boost{
namespace move_upmu {

//Compile time switch between
//single element, unknown bound array
//and known bound array
template<class T>
struct unique_ptr_if
{
   typedef ::boost::movelib::unique_ptr<T> t_is_not_array;
};

template<class T>
struct unique_ptr_if<T[]>
{
   typedef ::boost::movelib::unique_ptr<T[]> t_is_array_of_unknown_bound;
};

template<class T, std::size_t N>
struct unique_ptr_if<T[N]>
{
   typedef void t_is_array_of_known_bound;
};

template <int Dummy = 0>
struct nothrow_holder
{
   static std::nothrow_t *pnothrow;   
};

template <int Dummy>
std::nothrow_t *nothrow_holder<Dummy>::pnothrow = 
   reinterpret_cast<std::nothrow_t *>(0x1234);  //Avoid reference to null errors in sanitizers

}  //namespace move_upmu {
}  //namespace boost{

#endif   //!defined(BOOST_MOVE_DOXYGEN_INVOKED)

namespace boost{
namespace movelib {

#if defined(BOOST_MOVE_DOXYGEN_INVOKED) || !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is not an array.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new T(std::forward<Args>(args)...))</tt>.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array)
      make_unique(BOOST_FWD_REF(Args)... args)
{  return unique_ptr<T>(new T(::boost::forward<Args>(args)...));  }

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is not an array.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new T(std::nothrow)(std::forward<Args>(args)...))</tt>.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array)
      make_unique_nothrow(BOOST_FWD_REF(Args)... args)
{  return unique_ptr<T>(new (*boost::move_upmu::nothrow_holder<>::pnothrow)T(::boost::forward<Args>(args)...));  }

#else
   #define BOOST_MOVE_MAKE_UNIQUE_CODE(N)\
      template<class T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
      typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array\
         make_unique( BOOST_MOVE_UREF##N)\
      {  return unique_ptr<T>( new T( BOOST_MOVE_FWD##N ) );  }\
      \
      template<class T BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
      typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array\
         make_unique_nothrow( BOOST_MOVE_UREF##N)\
      {  return unique_ptr<T>( new (*boost::move_upmu::nothrow_holder<>::pnothrow)T ( BOOST_MOVE_FWD##N ) );  }\
      //
   BOOST_MOVE_ITERATE_0TO9(BOOST_MOVE_MAKE_UNIQUE_CODE)
   #undef BOOST_MOVE_MAKE_UNIQUE_CODE

#endif

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is not an array.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new T)</tt> (default initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array)
      make_unique_definit()
{
    return unique_ptr<T>(new T);
}

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is not an array.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new T(std::nothrow)</tt> (default initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_not_array)
      make_unique_nothrow_definit()
{
    return unique_ptr<T>(new (*boost::move_upmu::nothrow_holder<>::pnothrow)T);
}

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is an array of 
//!   unknown bound.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new remove_extent_t<T>[n]())</tt> (value initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_unknown_bound)
      make_unique(std::size_t n)
{
    typedef typename ::boost::move_upmu::remove_extent<T>::type U;
    return unique_ptr<T>(new U[n]());
}

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is an array of 
//!   unknown bound.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new (std::nothrow)remove_extent_t<T>[n]())</tt> (value initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_unknown_bound)
      make_unique_nothrow(std::size_t n)
{
    typedef typename ::boost::move_upmu::remove_extent<T>::type U;
    return unique_ptr<T>(new (*boost::move_upmu::nothrow_holder<>::pnothrow)U[n]());
}

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is an array of 
//!   unknown bound.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new remove_extent_t<T>[n])</tt> (default initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_unknown_bound)
      make_unique_definit(std::size_t n)
{
    typedef typename ::boost::move_upmu::remove_extent<T>::type U;
    return unique_ptr<T>(new U[n]);
}

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is an array of 
//!   unknown bound.
//!
//! <b>Returns</b>: <tt>unique_ptr<T>(new (std::nothrow)remove_extent_t<T>[n])</tt> (default initialization)
template<class T>
inline BOOST_MOVE_DOC1ST(unique_ptr<T>, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_unknown_bound)
      make_unique_nothrow_definit(std::size_t n)
{
    typedef typename ::boost::move_upmu::remove_extent<T>::type U;
    return unique_ptr<T>(new (*boost::move_upmu::nothrow_holder<>::pnothrow) U[n]);
}

#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is
//!   an array of known bound.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unspecified, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_known_bound)
      make_unique(BOOST_FWD_REF(Args) ...) = delete;

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is
//!   an array of known bound.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unspecified, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_known_bound)
      make_unique_definit(BOOST_FWD_REF(Args) ...) = delete;

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is
//!   an array of known bound.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unspecified, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_known_bound)
      make_unique_nothrow(BOOST_FWD_REF(Args) ...) = delete;

//! <b>Remarks</b>: This function shall not participate in overload resolution unless T is
//!   an array of known bound.
template<class T, class... Args>
inline BOOST_MOVE_DOC1ST(unspecified, 
   typename ::boost::move_upmu::unique_ptr_if<T>::t_is_array_of_known_bound)
      make_unique_nothrow_definit(BOOST_FWD_REF(Args) ...) = delete;

#endif

}  //namespace movelib {

}  //namespace boost{

#include <boost/move/detail/config_end.hpp>

#endif   //#ifndef BOOST_MOVE_MAKE_UNIQUE_HPP_INCLUDED
