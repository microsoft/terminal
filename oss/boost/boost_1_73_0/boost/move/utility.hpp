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
//! This header includes core utilities from <tt><boost/move/utility_core.hpp></tt> and defines
//! some more advanced utilities such as:

#ifndef BOOST_MOVE_MOVE_UTILITY_HPP
#define BOOST_MOVE_MOVE_UTILITY_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/workaround.hpp>  //forceinline
#include <boost/move/utility_core.hpp>
#include <boost/move/traits.hpp>

#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_MOVE_DOXYGEN_INVOKED)

   namespace boost {

   //////////////////////////////////////////////////////////////////////////////
   //
   //                            move_if_noexcept()
   //
   //////////////////////////////////////////////////////////////////////////////

   template <class T>
   BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
      < enable_move_utility_emulation<T>::value && !has_move_emulation_enabled<T>::value
      , typename ::boost::move_detail::add_const<T>::type &
      >::type
         move_if_noexcept(T& x) BOOST_NOEXCEPT
   {
      return x;
   }

   template <class T>
   BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
      < enable_move_utility_emulation<T>::value && has_move_emulation_enabled<T>::value
            && ::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value, rv<T>&>::type
         move_if_noexcept(T& x) BOOST_NOEXCEPT
   {
      return *static_cast<rv<T>* >(::boost::move_detail::addressof(x));
   }

   template <class T>
   BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
      < enable_move_utility_emulation<T>::value && has_move_emulation_enabled<T>::value
            && ::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value
      , rv<T>&
      >::type
         move_if_noexcept(rv<T>& x) BOOST_NOEXCEPT
   {
      return x;
   }

   template <class T>
   BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
      < enable_move_utility_emulation<T>::value && has_move_emulation_enabled<T>::value
            && !::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value
      , typename ::boost::move_detail::add_const<T>::type &
      >::type
         move_if_noexcept(T& x) BOOST_NOEXCEPT
   {
      return x;
   }

   template <class T>
   BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
      < enable_move_utility_emulation<T>::value && has_move_emulation_enabled<T>::value
            && !::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value
      , typename ::boost::move_detail::add_const<T>::type &
      >::type
         move_if_noexcept(rv<T>& x) BOOST_NOEXCEPT
   {
      return x;
   }

   }  //namespace boost

#else    //#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_MOVE_DOXYGEN_INVOKED)

   #if defined(BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE)
      #include <utility>

      namespace boost{

      using ::std::move_if_noexcept;

      }  //namespace boost

   #else //!BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE

      namespace boost {

      //////////////////////////////////////////////////////////////////////////////
      //
      //                            move_if_noexcept()
      //
      //////////////////////////////////////////////////////////////////////////////
      #if defined(BOOST_MOVE_DOXYGEN_INVOKED)
         //! This function provides a way to convert a reference into a rvalue reference
         //! in compilers with rvalue references. For other compilers converts T & into
         //! <i>::boost::rv<T> &</i> so that move emulation is activated. Reference
         //! would be converted to rvalue reference only if input type is nothrow move
         //! constructible or if it has no copy constructor. In all other cases const
         //! reference would be returned
         template <class T>
         rvalue_reference_or_const_lvalue_reference move_if_noexcept(input_reference) noexcept;

      #else //BOOST_MOVE_DOXYGEN_INVOKED

         template <class T>
         BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
            < ::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value, T&&>::type
               move_if_noexcept(T& x) BOOST_NOEXCEPT
         {  return ::boost::move(x);   }

         template <class T>
         BOOST_MOVE_FORCEINLINE typename ::boost::move_detail::enable_if_c
            < !::boost::move_detail::is_nothrow_move_constructible_or_uncopyable<T>::value, const T&>::type
               move_if_noexcept(T& x) BOOST_NOEXCEPT
         {  return x;  }

      #endif //BOOST_MOVE_DOXYGEN_INVOKED

      }  //namespace boost {

   #endif   //#if defined(BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE)

#endif   //BOOST_NO_CXX11_RVALUE_REFERENCES

#include <boost/move/detail/config_end.hpp>

#endif //#ifndef BOOST_MOVE_MOVE_UTILITY_HPP
