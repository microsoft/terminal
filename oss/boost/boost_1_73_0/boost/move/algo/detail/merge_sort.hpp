//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2016.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

//! \file

#ifndef BOOST_MOVE_DETAIL_MERGE_SORT_HPP
#define BOOST_MOVE_DETAIL_MERGE_SORT_HPP

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
#include <boost/move/algo/move.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/detail/destruct_n.hpp>
#include <boost/move/algo/detail/insertion_sort.hpp>
#include <cassert>

namespace boost {
namespace movelib {

// @cond

static const unsigned MergeSortInsertionSortThreshold = 16;

template <class RandIt, class Compare>
void inplace_stable_sort(RandIt first, RandIt last, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   if (size_type(last - first) <= size_type(MergeSortInsertionSortThreshold)) {
      insertion_sort(first, last, comp);
      return;
   }
   RandIt middle = first + (last - first) / 2;
   inplace_stable_sort(first, middle, comp);
   inplace_stable_sort(middle, last, comp);
   merge_bufferless_ONlogN_recursive
      (first, middle, last, size_type(middle - first), size_type(last - middle), comp);
}

// @endcond

template<class RandIt, class RandIt2, class Compare>
void merge_sort_copy( RandIt first, RandIt last
                   , RandIt2 dest, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;

   size_type const count = size_type(last - first);
   if(count <= MergeSortInsertionSortThreshold){
      insertion_sort_copy(first, last, dest, comp);
   }
   else{
      size_type const half = count/2;
      merge_sort_copy(first + half, last        , dest+half   , comp);
      merge_sort_copy(first       , first + half, first + half, comp);
      merge_with_right_placed
         ( first + half, first + half + half
         , dest, dest+half, dest + count
         , comp);
   }
}

template<class RandIt, class RandItRaw, class Compare>
void merge_sort_uninitialized_copy( RandIt first, RandIt last
                                 , RandItRaw uninitialized
                                 , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   typedef typename iterator_traits<RandIt>::value_type value_type;

   size_type const count = size_type(last - first);
   if(count <= MergeSortInsertionSortThreshold){
      insertion_sort_uninitialized_copy(first, last, uninitialized, comp);
   }
   else{
      size_type const half = count/2;
      merge_sort_uninitialized_copy(first + half, last, uninitialized + half, comp);
      destruct_n<value_type, RandItRaw> d(uninitialized+half);
      d.incr(count-half);
      merge_sort_copy(first, first + half, first + half, comp);
      uninitialized_merge_with_right_placed
         ( first + half, first + half + half
         , uninitialized, uninitialized+half, uninitialized+count
         , comp);
      d.release();
   }
}

template<class RandIt, class RandItRaw, class Compare>
void merge_sort( RandIt first, RandIt last, Compare comp
               , RandItRaw uninitialized)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   typedef typename iterator_traits<RandIt>::value_type value_type;

   size_type const count = size_type(last - first);
   if(count <= MergeSortInsertionSortThreshold){
      insertion_sort(first, last, comp);
   }
   else{
      size_type const half = count/2;
      size_type const rest = count -  half;
      RandIt const half_it = first + half;
      RandIt const rest_it = first + rest;

      merge_sort_uninitialized_copy(half_it, last, uninitialized, comp);
      destruct_n<value_type, RandItRaw> d(uninitialized);
      d.incr(rest);
      merge_sort_copy(first, half_it, rest_it, comp);
      merge_with_right_placed
         ( uninitialized, uninitialized + rest
         , first, rest_it, last, antistable<Compare>(comp));
   }
}

///@cond

template<class RandIt, class RandItRaw, class Compare>
void merge_sort_with_constructed_buffer( RandIt first, RandIt last, Compare comp, RandItRaw buffer)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;

   size_type const count = size_type(last - first);
   if(count <= MergeSortInsertionSortThreshold){
      insertion_sort(first, last, comp);
   }
   else{
      size_type const half = count/2;
      size_type const rest = count -  half;
      RandIt const half_it = first + half;
      RandIt const rest_it = first + rest;

      merge_sort_copy(half_it, last, buffer, comp);
      merge_sort_copy(first, half_it, rest_it, comp);
      merge_with_right_placed
         (buffer, buffer + rest
         , first, rest_it, last, antistable<Compare>(comp));
   }
}

template<typename RandIt, typename Pointer,
         typename Distance, typename Compare>
void stable_sort_ONlogN_recursive(RandIt first, RandIt last, Pointer buffer, Distance buffer_size, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   if (size_type(last - first) <= size_type(MergeSortInsertionSortThreshold)) {
      insertion_sort(first, last, comp);
   }
   else {
      const size_type len = (last - first) / 2;
      const RandIt middle = first + len;
      if (len > ((buffer_size+1)/2)){
         stable_sort_ONlogN_recursive(first, middle, buffer, buffer_size, comp);
         stable_sort_ONlogN_recursive(middle, last, buffer, buffer_size, comp);
      }
      else{
         merge_sort_with_constructed_buffer(first, middle, comp, buffer);
         merge_sort_with_constructed_buffer(middle, last, comp, buffer);
      }
      merge_adaptive_ONlogN_recursive(first, middle, last,
         size_type(middle - first),
         size_type(last - middle),
         buffer, buffer_size,
         comp);
   }
}

template<typename BidirectionalIterator, typename Compare, typename RandRawIt>
void stable_sort_adaptive_ONlogN2(BidirectionalIterator first,
		                           BidirectionalIterator last,
		                           Compare comp,
                                 RandRawIt uninitialized,
                                 std::size_t uninitialized_len)
{
   typedef typename iterator_traits<BidirectionalIterator>::value_type  value_type;

   ::boost::movelib::adaptive_xbuf<value_type, RandRawIt> xbuf(uninitialized, uninitialized_len);
   xbuf.initialize_until(uninitialized_len, *first);
   stable_sort_ONlogN_recursive(first, last, uninitialized, uninitialized_len, comp);
}

///@endcond

}} //namespace boost {  namespace movelib{

#include <boost/move/detail/config_end.hpp>

#endif //#ifndef BOOST_MOVE_DETAIL_MERGE_SORT_HPP
