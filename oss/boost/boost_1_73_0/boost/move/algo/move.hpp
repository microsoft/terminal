//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2012-2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_ALGO_MOVE_HPP
#define BOOST_MOVE_ALGO_MOVE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/detail/config_begin.hpp>

#include <boost/move/utility_core.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#include <boost/core/no_exceptions_support.hpp>

namespace boost {

//////////////////////////////////////////////////////////////////////////////
//
//                               move
//
//////////////////////////////////////////////////////////////////////////////

#if !defined(BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE)

   //! <b>Effects</b>: Moves elements in the range [first,last) into the range [result,result + (last -
   //!   first)) starting from first and proceeding to last. For each non-negative integer n < (last-first),
   //!   performs *(result + n) = ::boost::move (*(first + n)).
   //!
   //! <b>Effects</b>: result + (last - first).
   //!
   //! <b>Requires</b>: result shall not be in the range [first,last).
   //!
   //! <b>Complexity</b>: Exactly last - first move assignments.
   template <typename I, // I models InputIterator
            typename O> // O models OutputIterator
   O move(I f, I l, O result)
   {
      while (f != l) {
         *result = ::boost::move(*f);
         ++f; ++result;
      }
      return result;
   }

   //////////////////////////////////////////////////////////////////////////////
   //
   //                               move_backward
   //
   //////////////////////////////////////////////////////////////////////////////

   //! <b>Effects</b>: Moves elements in the range [first,last) into the range
   //!   [result - (last-first),result) starting from last - 1 and proceeding to
   //!   first. For each positive integer n <= (last - first),
   //!   performs *(result - n) = ::boost::move(*(last - n)).
   //!
   //! <b>Requires</b>: result shall not be in the range [first,last).
   //!
   //! <b>Returns</b>: result - (last - first).
   //!
   //! <b>Complexity</b>: Exactly last - first assignments.
   template <typename I, // I models BidirectionalIterator
   typename O> // O models BidirectionalIterator
   O move_backward(I f, I l, O result)
   {
      while (f != l) {
         --l; --result;
         *result = ::boost::move(*l);
      }
      return result;
   }

#else

   using ::std::move_backward;

#endif   //!defined(BOOST_MOVE_USE_STANDARD_LIBRARY_MOVE)

//////////////////////////////////////////////////////////////////////////////
//
//                               uninitialized_move
//
//////////////////////////////////////////////////////////////////////////////

//! <b>Effects</b>:
//!   \code
//!   for (; first != last; ++result, ++first)
//!      new (static_cast<void*>(&*result))
//!         typename iterator_traits<ForwardIterator>::value_type(boost::move(*first));
//!   \endcode
//!
//! <b>Returns</b>: result
template
   <typename I, // I models InputIterator
    typename F> // F models ForwardIterator
F uninitialized_move(I f, I l, F r
   /// @cond
//   ,typename ::boost::move_detail::enable_if<has_move_emulation_enabled<typename boost::movelib::iterator_traits<I>::value_type> >::type* = 0
   /// @endcond
   )
{
   typedef typename boost::movelib::iterator_traits<I>::value_type input_value_type;

   F back = r;
   BOOST_TRY{
      while (f != l) {
         void * const addr = static_cast<void*>(::boost::move_detail::addressof(*r));
         ::new(addr) input_value_type(::boost::move(*f));
         ++f; ++r;
      }
   }
   BOOST_CATCH(...){
      for (; back != r; ++back){
         boost::movelib::iterator_to_raw_pointer(back)->~input_value_type();
      }
      BOOST_RETHROW;
   }
   BOOST_CATCH_END
   return r;
}

/// @cond
/*
template
   <typename I,   // I models InputIterator
    typename F>   // F models ForwardIterator
F uninitialized_move(I f, I l, F r,
   typename ::boost::move_detail::disable_if<has_move_emulation_enabled<typename boost::movelib::iterator_traits<I>::value_type> >::type* = 0)
{
   return std::uninitialized_copy(f, l, r);
}
*/

/// @endcond

}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif //#ifndef BOOST_MOVE_ALGO_MOVE_HPP
