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

#ifndef BOOST_MOVE_ALGORITHM_HPP
#define BOOST_MOVE_ALGORITHM_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif
#
#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/move/detail/config_begin.hpp>

#include <boost/move/utility_core.hpp>
#include <boost/move/iterator.hpp>
#include <boost/move/algo/move.hpp>
#include <boost/core/no_exceptions_support.hpp>

#include <algorithm> //copy, copy_backward
#include <memory>    //uninitialized_copy

namespace boost {

//////////////////////////////////////////////////////////////////////////////
//
//                            uninitialized_copy_or_move
//
//////////////////////////////////////////////////////////////////////////////

namespace move_detail {

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F uninitialized_move_move_iterator(I f, I l, F r
//                             ,typename ::boost::move_detail::enable_if< has_move_emulation_enabled<typename I::value_type> >::type* = 0
)
{
   return ::boost::uninitialized_move(f, l, r);
}
/*
template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
F uninitialized_move_move_iterator(I f, I l, F r,
                                   typename ::boost::move_detail::disable_if< has_move_emulation_enabled<typename I::value_type> >::type* = 0)
{
   return std::uninitialized_copy(f.base(), l.base(), r);
}
*/
}  //namespace move_detail {

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F uninitialized_copy_or_move(I f, I l, F r,
                             typename ::boost::move_detail::enable_if< move_detail::is_move_iterator<I> >::type* = 0)
{
   return ::boost::move_detail::uninitialized_move_move_iterator(f, l, r);
}

//////////////////////////////////////////////////////////////////////////////
//
//                            copy_or_move
//
//////////////////////////////////////////////////////////////////////////////

namespace move_detail {

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F move_move_iterator(I f, I l, F r
//                             ,typename ::boost::move_detail::enable_if< has_move_emulation_enabled<typename I::value_type> >::type* = 0
)
{
   return ::boost::move(f, l, r);
}
/*
template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
F move_move_iterator(I f, I l, F r,
                                   typename ::boost::move_detail::disable_if< has_move_emulation_enabled<typename I::value_type> >::type* = 0)
{
   return std::copy(f.base(), l.base(), r);
}
*/

}  //namespace move_detail {

template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F copy_or_move(I f, I l, F r,
                             typename ::boost::move_detail::enable_if< move_detail::is_move_iterator<I> >::type* = 0)
{
   return ::boost::move_detail::move_move_iterator(f, l, r);
}

/// @endcond

//! <b>Effects</b>:
//!   \code
//!   for (; first != last; ++result, ++first)
//!      new (static_cast<void*>(&*result))
//!         typename iterator_traits<ForwardIterator>::value_type(*first);
//!   \endcode
//!
//! <b>Returns</b>: result
//!
//! <b>Note</b>: This function is provided because
//!   <i>std::uninitialized_copy</i> from some STL implementations
//!    is not compatible with <i>move_iterator</i>
template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F uninitialized_copy_or_move(I f, I l, F r
   /// @cond
   ,typename ::boost::move_detail::disable_if< move_detail::is_move_iterator<I> >::type* = 0
   /// @endcond
   )
{
   return std::uninitialized_copy(f, l, r);
}

//! <b>Effects</b>:
//!   \code
//!   for (; first != last; ++result, ++first)
//!      *result = *first;
//!   \endcode
//!
//! <b>Returns</b>: result
//!
//! <b>Note</b>: This function is provided because
//!   <i>std::uninitialized_copy</i> from some STL implementations
//!    is not compatible with <i>move_iterator</i>
template
<typename I,   // I models InputIterator
typename F>   // F models ForwardIterator
inline F copy_or_move(I f, I l, F r
   /// @cond
   ,typename ::boost::move_detail::disable_if< move_detail::is_move_iterator<I> >::type* = 0
   /// @endcond
   )
{
   return std::copy(f, l, r);
}

}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif //#ifndef BOOST_MOVE_ALGORITHM_HPP
