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

#ifndef BOOST_MOVE_ADAPTIVE_MERGE_HPP
#define BOOST_MOVE_ADAPTIVE_MERGE_HPP

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/algo/detail/adaptive_sort_merge.hpp>

namespace boost {
namespace movelib {

///@cond
namespace detail_adaptive {

template<class RandIt, class Compare, class XBuf>
inline void adaptive_merge_combine_blocks( RandIt first
                                      , typename iterator_traits<RandIt>::size_type len1
                                      , typename iterator_traits<RandIt>::size_type len2
                                      , typename iterator_traits<RandIt>::size_type collected
                                      , typename iterator_traits<RandIt>::size_type n_keys
                                      , typename iterator_traits<RandIt>::size_type l_block
                                      , bool use_internal_buf
                                      , bool xbuf_used
                                      , Compare comp
                                      , XBuf & xbuf
                                      )
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const len = len1+len2;
   size_type const l_combine  = len-collected;
   size_type const l_combine1 = len1-collected;

    if(n_keys){
      RandIt const first_data = first+collected;
      RandIt const keys = first;
      BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   A combine: ", len);
      if(xbuf_used){
         if(xbuf.size() < l_block){
            xbuf.initialize_until(l_block, *first);
         }
         BOOST_ASSERT(xbuf.size() >= l_block);
         size_type n_block_a, n_block_b, l_irreg1, l_irreg2;
         combine_params( keys, comp, l_combine
                           , l_combine1, l_block, xbuf
                           , n_block_a, n_block_b, l_irreg1, l_irreg2);   //Outputs
         op_merge_blocks_with_buf
            (keys, comp, first_data, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp, move_op(), xbuf.data());
         BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1("   A mrg xbf: ", len);
      }
      else{
         size_type n_block_a, n_block_b, l_irreg1, l_irreg2;
         combine_params( keys, comp, l_combine
                           , l_combine1, l_block, xbuf
                           , n_block_a, n_block_b, l_irreg1, l_irreg2);   //Outputs
         if(use_internal_buf){
            op_merge_blocks_with_buf
               (keys, comp, first_data, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp, swap_op(), first_data-l_block);
            BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   A mrg buf: ", len);
         }
         else{
            merge_blocks_bufferless
               (keys, comp, first_data, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp);
            BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1("   A mrg nbf: ", len);
         }
      }
   }
   else{
      xbuf.shrink_to_fit(l_block);
      if(xbuf.size() < l_block){
         xbuf.initialize_until(l_block, *first);
      }
      size_type *const uint_keys = xbuf.template aligned_trailing<size_type>(l_block);
      size_type n_block_a, n_block_b, l_irreg1, l_irreg2;
      combine_params( uint_keys, less(), l_combine
                     , l_combine1, l_block, xbuf
                     , n_block_a, n_block_b, l_irreg1, l_irreg2, true);   //Outputs
      BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   A combine: ", len);
      BOOST_ASSERT(xbuf.size() >= l_block);
      op_merge_blocks_with_buf
         (uint_keys, less(), first, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp, move_op(), xbuf.data());
      xbuf.clear();
      BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1("   A mrg buf: ", len);
   }
}

template<class RandIt, class Compare, class XBuf>
inline void adaptive_merge_final_merge( RandIt first
                                      , typename iterator_traits<RandIt>::size_type len1
                                      , typename iterator_traits<RandIt>::size_type len2
                                      , typename iterator_traits<RandIt>::size_type collected
                                      , typename iterator_traits<RandIt>::size_type l_intbuf
                                      , typename iterator_traits<RandIt>::size_type l_block
                                      , bool use_internal_buf
                                      , bool xbuf_used
                                      , Compare comp
                                      , XBuf & xbuf
                                      )
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   (void)l_block;
   (void)use_internal_buf;
   size_type n_keys = collected-l_intbuf;
   size_type len = len1+len2;
   if (!xbuf_used || n_keys) {
      xbuf.clear();
      const size_type middle = xbuf_used && n_keys ? n_keys: collected;
      unstable_sort(first, first + middle, comp, xbuf);
      BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   A k/b srt: ", len);
      stable_merge(first, first + middle, first + len, comp, xbuf);
   }
   BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1("   A fin mrg: ", len);
}

template<class SizeType>
inline static SizeType adaptive_merge_n_keys_without_external_keys(SizeType l_block, SizeType len1, SizeType len2, SizeType l_intbuf)
{
   typedef SizeType size_type;
   //This is the minimum number of keys to implement the ideal algorithm
   size_type n_keys = len1/l_block+len2/l_block;
   const size_type second_half_blocks = len2/l_block;
   const size_type first_half_aux = len1-l_intbuf;
   while(n_keys >= ((first_half_aux-n_keys)/l_block + second_half_blocks)){
      --n_keys;
   }
   ++n_keys;
   return n_keys;
}

template<class SizeType>
inline static SizeType adaptive_merge_n_keys_with_external_keys(SizeType l_block, SizeType len1, SizeType len2, SizeType l_intbuf)
{
   typedef SizeType size_type;
   //This is the minimum number of keys to implement the ideal algorithm
   size_type n_keys = (len1-l_intbuf)/l_block + len2/l_block;
   return n_keys;
}

template<class SizeType, class Xbuf>
inline SizeType adaptive_merge_n_keys_intbuf(SizeType &rl_block, SizeType len1, SizeType len2, Xbuf & xbuf, SizeType &l_intbuf_inout)
{
   typedef SizeType size_type;
   size_type l_block = rl_block;
   size_type l_intbuf = xbuf.capacity() >= l_block ? 0u : l_block;

   if (xbuf.capacity() > l_block){
      l_block = xbuf.capacity();
   }

   //This is the minimum number of keys to implement the ideal algorithm
   size_type n_keys = adaptive_merge_n_keys_without_external_keys(l_block, len1, len2, l_intbuf);
   BOOST_ASSERT(n_keys >= ((len1-l_intbuf-n_keys)/l_block + len2/l_block));

   if(xbuf.template supports_aligned_trailing<size_type>
      ( l_block
      , adaptive_merge_n_keys_with_external_keys(l_block, len1, len2, l_intbuf)))
   {
      n_keys = 0u;
   }
   l_intbuf_inout = l_intbuf;
   rl_block = l_block;
   return n_keys;
}

// Main explanation of the merge algorithm.
//
// csqrtlen = ceil(sqrt(len));
//
// * First, csqrtlen [to be used as buffer] + (len/csqrtlen - 1) [to be used as keys] => to_collect
//   unique elements are extracted from elements to be sorted and placed in the beginning of the range.
//
// * Step "combine_blocks": the leading (len1-to_collect) elements plus trailing len2 elements
//   are merged with a non-trivial ("smart") algorithm to form an ordered range trailing "len-to_collect" elements.
//
//   Explanation of the "combine_blocks" step:
//
//         * Trailing [first+to_collect, first+len1) elements are divided in groups of cqrtlen elements.
//           Remaining elements that can't form a group are grouped in front of those elements.
//         * Trailing [first+len1, first+len1+len2) elements are divided in groups of cqrtlen elements.
//           Remaining elements that can't form a group are grouped in the back of those elements.
//         * In parallel the following two steps are performed:
//             *  Groups are selection-sorted by first or last element (depending whether they are going
//                to be merged to left or right) and keys are reordered accordingly as an imitation-buffer.
//             * Elements of each block pair are merged using the csqrtlen buffer taking into account
//                if they belong to the first half or second half (marked by the key).
//
// * In the final merge step leading "to_collect" elements are merged with rotations
//   with the rest of merged elements in the "combine_blocks" step.
//
// Corner cases:
//
// * If no "to_collect" elements can be extracted:
//
//    * If more than a minimum number of elements is extracted
//      then reduces the number of elements used as buffer and keys in the
//      and "combine_blocks" steps. If "combine_blocks" has no enough keys due to this reduction
//      then uses a rotation based smart merge.
//
//    * If the minimum number of keys can't be extracted, a rotation-based merge is performed.
//
// * If auxiliary memory is more or equal than min(len1, len2), a buffered merge is performed.
//
// * If the len1 or len2 are less than 2*csqrtlen then a rotation-based merge is performed.
//
// * If auxiliary memory is more than csqrtlen+n_keys*sizeof(std::size_t),
//   then no csqrtlen need to be extracted and "combine_blocks" will use integral
//   keys to combine blocks.
template<class RandIt, class Compare, class XBuf>
void adaptive_merge_impl
   ( RandIt first
   , typename iterator_traits<RandIt>::size_type len1
   , typename iterator_traits<RandIt>::size_type len2
   , Compare comp
   , XBuf & xbuf
   )
{
   typedef typename iterator_traits<RandIt>::size_type size_type;

   if(xbuf.capacity() >= min_value<size_type>(len1, len2)){
      buffered_merge(first, first+len1, first+(len1+len2), comp, xbuf);
   }
   else{
      const size_type len = len1+len2;
      //Calculate ideal parameters and try to collect needed unique keys
      size_type l_block = size_type(ceil_sqrt(len));

      //One range is not big enough to extract keys and the internal buffer so a
      //rotation-based based merge will do just fine
      if(len1 <= l_block*2 || len2 <= l_block*2){
         merge_bufferless(first, first+len1, first+len1+len2, comp);
         return;
      }

      //Detail the number of keys and internal buffer. If xbuf has enough memory, no
      //internal buffer is needed so l_intbuf will remain 0.
      size_type l_intbuf = 0;
      size_type n_keys = adaptive_merge_n_keys_intbuf(l_block, len1, len2, xbuf, l_intbuf);
      size_type const to_collect = l_intbuf+n_keys;
      //Try to extract needed unique values from the first range
      size_type const collected  = collect_unique(first, first+len1, to_collect, comp, xbuf);
      BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1("\n   A collect: ", len);

      //Not the minimum number of keys is not available on the first range, so fallback to rotations
      if(collected != to_collect && collected < 4){
         merge_bufferless(first, first+collected, first+len1, comp);
         merge_bufferless(first, first + len1, first + len1 + len2, comp);
         return;
      }

      //If not enough keys but more than minimum, adjust the internal buffer and key count
      bool use_internal_buf = collected == to_collect;
      if (!use_internal_buf){
         l_intbuf = 0u;
         n_keys = collected;
         l_block  = lblock_for_combine(l_intbuf, n_keys, len, use_internal_buf);
         //If use_internal_buf is false, then then internal buffer will be zero and rotation-based combination will be used
         l_intbuf = use_internal_buf ? l_block : 0u;
      }

      bool const xbuf_used = collected == to_collect && xbuf.capacity() >= l_block;
      //Merge trailing elements using smart merges
      adaptive_merge_combine_blocks(first, len1, len2, collected,   n_keys, l_block, use_internal_buf, xbuf_used, comp, xbuf);
      //Merge buffer and keys with the rest of the values
      adaptive_merge_final_merge   (first, len1, len2, collected, l_intbuf, l_block, use_internal_buf, xbuf_used, comp, xbuf);
   }
}

}  //namespace detail_adaptive {

///@endcond

//! <b>Effects</b>: Merges two consecutive sorted ranges [first, middle) and [middle, last)
//!   into one sorted range [first, last) according to the given comparison function comp.
//!   The algorithm is stable (if there are equivalent elements in the original two ranges,
//!   the elements from the first range (preserving their original order) precede the elements
//!   from the second range (preserving their original order).
//!
//! <b>Requires</b>:
//!   - RandIt must meet the requirements of ValueSwappable and RandomAccessIterator.
//!   - The type of dereferenced RandIt must meet the requirements of MoveAssignable and MoveConstructible.
//!
//! <b>Parameters</b>:
//!   - first: the beginning of the first sorted range. 
//!   - middle: the end of the first sorted range and the beginning of the second
//!   - last: the end of the second sorted range
//!   - comp: comparison function object which returns true if the first argument is is ordered before the second.
//!   - uninitialized, uninitialized_len: raw storage starting on "uninitialized", able to hold "uninitialized_len"
//!      elements of type iterator_traits<RandIt>::value_type. Maximum performance is achieved when uninitialized_len
//!      is min(std::distance(first, middle), std::distance(middle, last)).
//!
//! <b>Throws</b>: If comp throws or the move constructor, move assignment or swap of the type
//!   of dereferenced RandIt throws.
//!
//! <b>Complexity</b>: Always K x O(N) comparisons and move assignments/constructors/swaps.
//!   Constant factor for comparisons and data movement is minimized when uninitialized_len
//!   is min(std::distance(first, middle), std::distance(middle, last)).
//!   Pretty good enough performance is achieved when uninitialized_len is
//!   ceil(sqrt(std::distance(first, last)))*2.
//!
//! <b>Caution</b>: Experimental implementation, not production-ready.
template<class RandIt, class Compare>
void adaptive_merge( RandIt first, RandIt middle, RandIt last, Compare comp
                , typename iterator_traits<RandIt>::value_type* uninitialized = 0
                , typename iterator_traits<RandIt>::size_type uninitialized_len = 0)
{
   typedef typename iterator_traits<RandIt>::size_type  size_type;
   typedef typename iterator_traits<RandIt>::value_type value_type;

   if (first == middle || middle == last){
      return;
   }

   //Reduce ranges to merge if possible
   do {
      if (comp(*middle, *first)){
         break;
      }
      ++first;
      if (first == middle)
         return;
   } while(1);

   RandIt first_high(middle);
   --first_high;
   do {
      --last;
      if (comp(*last, *first_high)){
         ++last;
         break;
      }
      if (last == middle)
         return;
   } while(1);

   ::boost::movelib::adaptive_xbuf<value_type, value_type*, size_type> xbuf(uninitialized, size_type(uninitialized_len));
   ::boost::movelib::detail_adaptive::adaptive_merge_impl(first, size_type(middle - first), size_type(last - middle), comp, xbuf);
}

}  //namespace movelib {
}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif   //#define BOOST_MOVE_ADAPTIVE_MERGE_HPP
