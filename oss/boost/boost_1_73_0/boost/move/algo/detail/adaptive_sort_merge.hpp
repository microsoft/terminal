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
//
// Stable sorting that works in O(N*log(N)) worst time
// and uses O(1) extra memory
//
//////////////////////////////////////////////////////////////////////////////
//
// The main idea of the adaptive_sort algorithm was developed by Andrey Astrelin
// and explained in the article from the russian collaborative blog
// Habrahabr (http://habrahabr.ru/post/205290/). The algorithm is based on
// ideas from B-C. Huang and M. A. Langston explained in their article
// "Fast Stable Merging and Sorting in Constant Extra Space (1989-1992)"
// (http://comjnl.oxfordjournals.org/content/35/6/643.full.pdf).
//
// This implementation by Ion Gaztanaga uses previous ideas with additional changes:
// 
// - Use of GCD-based rotation.
// - Non power of two buffer-sizes.
// - Tries to find sqrt(len)*2 unique keys, so that the merge sort
//   phase can form up to sqrt(len)*4 segments if enough keys are found.
// - The merge-sort phase can take advantage of external memory to
//   save some additional combination steps.
// - Combination phase: Blocks are selection sorted and merged in parallel.
// - The combination phase is performed alternating merge to left and merge
//   to right phases minimizing swaps due to internal buffer repositioning.
// - When merging blocks special optimizations are made to avoid moving some
//   elements twice.
//
// The adaptive_merge algorithm was developed by Ion Gaztanaga reusing some parts
// from the sorting algorithm and implementing an additional block merge algorithm
// without moving elements to left or right.
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP
#define BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/reverse_iterator.hpp>
#include <boost/move/algo/move.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/algo/detail/insertion_sort.hpp>
#include <boost/move/algo/detail/merge_sort.hpp>
#include <boost/move/algo/detail/heap_sort.hpp>
#include <boost/move/algo/detail/merge.hpp>
#include <boost/move/algo/detail/is_sorted.hpp>
#include <boost/assert.hpp>
#include <boost/cstdint.hpp>

#ifndef BOOST_MOVE_ADAPTIVE_SORT_STATS_LEVEL
   #define BOOST_MOVE_ADAPTIVE_SORT_STATS_LEVEL 1
#endif

#ifdef BOOST_MOVE_ADAPTIVE_SORT_STATS
   #if BOOST_MOVE_ADAPTIVE_SORT_STATS_LEVEL == 2
      #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1(STR, L) \
         print_stats(STR, L)\
      //

      #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2(STR, L) \
         print_stats(STR, L)\
      //
   #else
      #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1(STR, L) \
         print_stats(STR, L)\
      //

      #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2(STR, L)
   #endif
#else
   #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L1(STR, L)
   #define BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2(STR, L)
#endif

#ifdef BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS
   #define BOOST_MOVE_ADAPTIVE_SORT_INVARIANT  BOOST_ASSERT
#else
   #define BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(L)
#endif

namespace boost {
namespace movelib {

#if defined(BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS)

bool is_sorted(::order_perf_type *first, ::order_perf_type *last, ::order_type_less)
{
   if (first != last) {
      const order_perf_type *next = first, *cur(first);
      while (++next != last) {
         if (!(cur->key < next->key || (cur->key == next->key && cur->val < next->val)))
            return false;
         cur = next;
      }
   }
   return true;
}

#endif   //BOOST_MOVE_ADAPTIVE_SORT_INVARIANTS

namespace detail_adaptive {

static const std::size_t AdaptiveSortInsertionSortThreshold = 16;
//static const std::size_t AdaptiveSortInsertionSortThreshold = 4;
BOOST_STATIC_ASSERT((AdaptiveSortInsertionSortThreshold&(AdaptiveSortInsertionSortThreshold-1)) == 0);

#if defined BOOST_HAS_INTPTR_T
   typedef ::boost::uintptr_t uintptr_t;
#else
   typedef std::size_t uintptr_t;
#endif

template<class T>
const T &min_value(const T &a, const T &b)
{
   return a < b ? a : b;
}

template<class T>
const T &max_value(const T &a, const T &b)
{
   return a > b ? a : b;
}

template<class ForwardIt, class Pred, class V>
typename iterator_traits<ForwardIt>::size_type
   count_if_with(ForwardIt first, ForwardIt last, Pred pred, const V &v)
{
   typedef typename iterator_traits<ForwardIt>::size_type size_type;
   size_type count = 0;
   while(first != last) {
      count += static_cast<size_type>(0 != pred(*first, v));
      ++first;
   }
   return count;
}


template<class RandIt, class Compare>
RandIt skip_until_merge
   ( RandIt first1, RandIt const last1
   , const typename iterator_traits<RandIt>::value_type &next_key, Compare comp)
{
   while(first1 != last1 && !comp(next_key, *first1)){
      ++first1;
   }
   return first1;
}


template<class RandItKeys, class RandIt>
void swap_and_update_key
   ( RandItKeys const key_next
   , RandItKeys const key_range2
   , RandItKeys &key_mid
   , RandIt const begin
   , RandIt const end
   , RandIt const with)
{
   if(begin != with){
      ::boost::adl_move_swap_ranges(begin, end, with);
      ::boost::adl_move_swap(*key_next, *key_range2);
      if(key_next == key_mid){
         key_mid = key_range2;
      }
      else if(key_mid == key_range2){
         key_mid = key_next;
      }
   }
}

template<class RandItKeys>
void update_key
(RandItKeys const key_next
   , RandItKeys const key_range2
   , RandItKeys &key_mid)
{
   if (key_next != key_range2) {
      ::boost::adl_move_swap(*key_next, *key_range2);
      if (key_next == key_mid) {
         key_mid = key_range2;
      }
      else if (key_mid == key_range2) {
         key_mid = key_next;
      }
   }
}

template<class RandItKeys, class RandIt, class RandIt2, class Op>
RandIt2 buffer_and_update_key
(RandItKeys const key_next
   , RandItKeys const key_range2
   , RandItKeys &key_mid
   , RandIt begin
   , RandIt end
   , RandIt with
   , RandIt2 buffer
   , Op op)
{
   if (begin != with) {
      while(begin != end) {
         op(three_way_t(), begin++, with++, buffer++);
      }
      ::boost::adl_move_swap(*key_next, *key_range2);
      if (key_next == key_mid) {
         key_mid = key_range2;
      }
      else if (key_mid == key_range2) {
         key_mid = key_next;
      }
   }
   return buffer;
}

///////////////////////////////////////////////////////////////////////////////
//
//                         MERGE BUFFERLESS
//
///////////////////////////////////////////////////////////////////////////////

// [first1, last1) merge [last1,last2) -> [first1,last2)
template<class RandIt, class Compare>
RandIt partial_merge_bufferless_impl
   (RandIt first1, RandIt last1, RandIt const last2, bool *const pis_range1_A, Compare comp)
{
   if(last1 == last2){
      return first1;
   }
   bool const is_range1_A = *pis_range1_A;
   if(first1 != last1 && comp(*last1, last1[-1])){
      do{
         RandIt const old_last1 = last1;
         last1  = boost::movelib::lower_bound(last1, last2, *first1, comp);
         first1 = rotate_gcd(first1, old_last1, last1);//old_last1 == last1 supported
         if(last1 == last2){
            return first1;
         }
         do{
            ++first1;
         } while(last1 != first1 && !comp(*last1, *first1) );
      } while(first1 != last1);
   }
   *pis_range1_A = !is_range1_A;
   return last1;
}

// [first1, last1) merge [last1,last2) -> [first1,last2)
template<class RandIt, class Compare>
RandIt partial_merge_bufferless
   (RandIt first1, RandIt last1, RandIt const last2, bool *const pis_range1_A, Compare comp)
{
   return *pis_range1_A ? partial_merge_bufferless_impl(first1, last1, last2, pis_range1_A, comp)
                        : partial_merge_bufferless_impl(first1, last1, last2, pis_range1_A, antistable<Compare>(comp));
}

template<class SizeType>
static SizeType needed_keys_count(SizeType n_block_a, SizeType n_block_b)
{
   return n_block_a + n_block_b;
}

template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type
   find_next_block
      ( RandItKeys const key_first
      , KeyCompare key_comp
      , RandIt const first
      , typename iterator_traits<RandIt>::size_type const l_block
      , typename iterator_traits<RandIt>::size_type const ix_first_block
      , typename iterator_traits<RandIt>::size_type const ix_last_block
      , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type      size_type;
   typedef typename iterator_traits<RandIt>::value_type     value_type;
   typedef typename iterator_traits<RandItKeys>::value_type key_type;
   BOOST_ASSERT(ix_first_block <= ix_last_block);
   size_type ix_min_block = 0u;
   for (size_type szt_i = ix_first_block; szt_i < ix_last_block; ++szt_i) {
      const value_type &min_val = first[ix_min_block*l_block];
      const value_type &cur_val = first[szt_i*l_block];
      const key_type   &min_key = key_first[ix_min_block];
      const key_type   &cur_key = key_first[szt_i];

      bool const less_than_minimum = comp(cur_val, min_val) ||
         (!comp(min_val, cur_val) && key_comp(cur_key, min_key));

      if (less_than_minimum) {
         ix_min_block = szt_i;
      }
   }
   return ix_min_block;
}

template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_bufferless
   ( RandItKeys const key_first
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_block_a
   , typename iterator_traits<RandIt>::size_type const n_block_b
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const key_count = needed_keys_count(n_block_a, n_block_b); (void)key_count;
   //BOOST_ASSERT(n_block_a || n_block_b);
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted_and_unique(key_first, key_first + key_count, key_comp));
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_b || n_block_a == count_if_with(key_first, key_first + key_count, key_comp, key_first[n_block_a]));

   size_type n_bef_irreg2 = 0;
   bool l_irreg_pos_count = true;
   RandItKeys key_mid(key_first + n_block_a);
   RandIt const first_irr2 = first + l_irreg1 + (n_block_a+n_block_b)*l_block;
   RandIt const last_irr2  = first_irr2 + l_irreg2;

   {  //Selection sort blocks
      size_type n_block_left = n_block_b + n_block_a;
      RandItKeys key_range2(key_first);

      size_type min_check = n_block_a == n_block_left ? 0u : n_block_a;
      size_type max_check = min_value<size_type>(min_check+1, n_block_left);
      for (RandIt f = first+l_irreg1; n_block_left; --n_block_left, ++key_range2, f += l_block, min_check -= min_check != 0, max_check -= max_check != 0) {
         size_type const next_key_idx = find_next_block(key_range2, key_comp, f, l_block, min_check, max_check, comp);
         RandItKeys const key_next(key_range2 + next_key_idx);
         max_check = min_value<size_type>(max_value<size_type>(max_check, next_key_idx+size_type(2)), n_block_left);

         RandIt const first_min = f + next_key_idx*l_block;

         //Check if irregular b block should go here.
         //If so, break to the special code handling the irregular block
         if (l_irreg_pos_count && l_irreg2 && comp(*first_irr2, *first_min)){
            l_irreg_pos_count = false;
         }
         n_bef_irreg2 += l_irreg_pos_count;

         swap_and_update_key(key_next, key_range2, key_mid, f, f + l_block, first_min);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(f, f+l_block, comp));
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first_min, first_min + l_block, comp));
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT((f == (first+l_irreg1)) || !comp(*f, *(f-l_block)));
      }
   }
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first+l_irreg1+n_bef_irreg2*l_block, first_irr2, comp));

   RandIt first1 = first;
   RandIt last1  = first+l_irreg1;
   RandItKeys const key_end (key_first+n_bef_irreg2);
   bool is_range1_A = true;

   for(RandItKeys key_next = key_first; key_next != key_end; ++key_next){
      bool is_range2_A = key_mid == (key_first+key_count) || key_comp(*key_next, *key_mid);
      first1 = is_range1_A == is_range2_A
         ? last1 : partial_merge_bufferless(first1, last1, last1 + l_block, &is_range1_A, comp);
      last1 += l_block;
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, first1, comp));
   }

   merge_bufferless(is_range1_A ? first1 : last1, first_irr2, last_irr2, comp);
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, last_irr2, comp));
}

// Complexity: 2*distance(first, last)+max_collected^2/2
//
// Tries to collect at most n_keys unique elements from [first, last),
// in the begining of the range, and ordered according to comp
// 
// Returns the number of collected keys
template<class RandIt, class Compare, class XBuf>
typename iterator_traits<RandIt>::size_type
   collect_unique
      ( RandIt const first, RandIt const last
      , typename iterator_traits<RandIt>::size_type const max_collected, Compare comp
      , XBuf & xbuf)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type h = 0;
   if(max_collected){
      ++h;  // first key is always here
      RandIt h0 = first;
      RandIt u = first; ++u;
      RandIt search_end = u;

      if(xbuf.capacity() >= max_collected){
         typename XBuf::iterator const ph0 = xbuf.add(first);
         while(u != last && h < max_collected){
            typename XBuf::iterator const r = boost::movelib::lower_bound(ph0, xbuf.end(), *u, comp);
            //If key not found add it to [h, h+h0)
            if(r == xbuf.end() || comp(*u, *r) ){
               RandIt const new_h0 = boost::move(search_end, u, h0);
               search_end = u;
               ++search_end;
               ++h;
               xbuf.insert(r, u);
               h0 = new_h0;
            }
            ++u;
         }
         boost::move_backward(first, h0, h0+h);
         boost::move(xbuf.data(), xbuf.end(), first);
      }
      else{
         while(u != last && h < max_collected){
            RandIt const r = boost::movelib::lower_bound(h0, search_end, *u, comp);
            //If key not found add it to [h, h+h0)
            if(r == search_end || comp(*u, *r) ){
               RandIt const new_h0 = rotate_gcd(h0, search_end, u);
               search_end = u;
               ++search_end;
               ++h;
               rotate_gcd(r+(new_h0-h0), u, search_end);
               h0 = new_h0;
            }
            ++u;
         }
         rotate_gcd(first, h0, h0+h);
      }
   }
   return h;
}

template<class Unsigned>
Unsigned floor_sqrt(Unsigned const n)
{
   Unsigned x = n;
   Unsigned y = x/2 + (x&1);
   while (y < x){
      x = y;
      y = (x + n / x)/2;
   }
   return x;
}

template<class Unsigned>
Unsigned ceil_sqrt(Unsigned const n)
{
   Unsigned r = floor_sqrt(n);
   return r + Unsigned((n%r) != 0);
}

template<class Unsigned>
Unsigned floor_merge_multiple(Unsigned const n, Unsigned &base, Unsigned &pow)
{
   Unsigned s = n;
   Unsigned p = 0;
   while(s > AdaptiveSortInsertionSortThreshold){
      s /= 2;
      ++p;
   }
   base = s;
   pow = p;
   return s << p;
}

template<class Unsigned>
Unsigned ceil_merge_multiple(Unsigned const n, Unsigned &base, Unsigned &pow)
{
   Unsigned fm = floor_merge_multiple(n, base, pow);

   if(fm != n){
      if(base < AdaptiveSortInsertionSortThreshold){
         ++base;
      }
      else{
         base = AdaptiveSortInsertionSortThreshold/2 + 1;
         ++pow;
      }
   }
   return base << pow;
}

template<class Unsigned>
Unsigned ceil_sqrt_multiple(Unsigned const n, Unsigned *pbase = 0)
{
   Unsigned const r = ceil_sqrt(n);
   Unsigned pow = 0;
   Unsigned base = 0;
   Unsigned const res = ceil_merge_multiple(r, base, pow);
   if(pbase) *pbase = base;
   return res;
}

struct less
{
   template<class T>
   bool operator()(const T &l, const T &r)
   {  return l < r;  }
};

///////////////////////////////////////////////////////////////////////////////
//
//                            MERGE BLOCKS
//
///////////////////////////////////////////////////////////////////////////////

//#define ADAPTIVE_SORT_MERGE_SLOW_STABLE_SORT_IS_NLOGN

#if defined ADAPTIVE_SORT_MERGE_SLOW_STABLE_SORT_IS_NLOGN
template<class RandIt, class Compare>
void slow_stable_sort
   ( RandIt const first, RandIt const last, Compare comp)
{
   boost::movelib::inplace_stable_sort(first, last, comp);
}

#else //ADAPTIVE_SORT_MERGE_SLOW_STABLE_SORT_IS_NLOGN

template<class RandIt, class Compare>
void slow_stable_sort
   ( RandIt const first, RandIt const last, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type L = size_type(last - first);
   {  //Use insertion sort to merge first elements
      size_type m = 0;
      while((L - m) > size_type(AdaptiveSortInsertionSortThreshold)){
         insertion_sort(first+m, first+m+size_type(AdaptiveSortInsertionSortThreshold), comp);
         m += AdaptiveSortInsertionSortThreshold;
      }
      insertion_sort(first+m, last, comp);
   }

   size_type h = AdaptiveSortInsertionSortThreshold;
   for(bool do_merge = L > h; do_merge; h*=2){
      do_merge = (L - h) > h;
      size_type p0 = 0;
      if(do_merge){
         size_type const h_2 = 2*h;
         while((L-p0) > h_2){
            merge_bufferless(first+p0, first+p0+h, first+p0+h_2, comp);
            p0 += h_2;
         }
      }
      if((L-p0) > h){
         merge_bufferless(first+p0, first+p0+h, last, comp);
      }
   }
}

#endif   //ADAPTIVE_SORT_MERGE_SLOW_STABLE_SORT_IS_NLOGN

//Returns new l_block and updates use_buf
template<class Unsigned>
Unsigned lblock_for_combine
   (Unsigned const l_block, Unsigned const n_keys, Unsigned const l_data, bool &use_buf)
{
   BOOST_ASSERT(l_data > 1);

   //We need to guarantee lblock >= l_merged/(n_keys/2) keys for the combination.
   //We have at least 4 keys guaranteed (which are the minimum to merge 2 ranges)
   //If l_block != 0, then n_keys is already enough to merge all blocks in all
   //phases as we've found all needed keys for that buffer and length before.
   //If l_block == 0 then see if half keys can be used as buffer and the rest
   //as keys guaranteeing that n_keys >= (2*l_merged)/lblock = 
   if(!l_block){
      //If l_block == 0 then n_keys is power of two
      //(guaranteed by build_params(...))
      BOOST_ASSERT(n_keys >= 4);
      //BOOST_ASSERT(0 == (n_keys &(n_keys-1)));

      //See if half keys are at least 4 and if half keys fulfill
      Unsigned const new_buf  = n_keys/2;
      Unsigned const new_keys = n_keys-new_buf;
      use_buf = new_keys >= 4 && new_keys >= l_data/new_buf;
      if(use_buf){
         return new_buf;
      }
      else{
         return l_data/n_keys;
      }
   }
   else{
      use_buf = true;
      return l_block;
   }
}

template<class RandIt, class Compare, class XBuf>
void stable_sort( RandIt first, RandIt last, Compare comp, XBuf & xbuf)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const len = size_type(last - first);
   size_type const half_len = len/2 + (len&1);
   if(std::size_t(xbuf.capacity() - xbuf.size()) >= half_len) {
      merge_sort(first, last, comp, xbuf.data()+xbuf.size());
   }
   else{
      slow_stable_sort(first, last, comp);
   }
}

template<class RandIt, class Comp, class XBuf>
void unstable_sort( RandIt first, RandIt last
                    , Comp comp
                    , XBuf & xbuf)
{
   heap_sort(first, last, comp);(void)xbuf;
}

template<class RandIt, class Compare, class XBuf>
void stable_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp
      , XBuf &xbuf)
{
   BOOST_ASSERT(xbuf.empty());
   typedef typename iterator_traits<RandIt>::size_type   size_type;
   size_type const len1  = size_type(middle-first);
   size_type const len2  = size_type(last-middle);
   size_type const l_min = min_value<size_type>(len1, len2);
   if(xbuf.capacity() >= l_min){
      buffered_merge(first, middle, last, comp, xbuf);
      xbuf.clear();
   }
   else{
      //merge_bufferless(first, middle, last, comp);
      merge_adaptive_ONlogN(first, middle, last, comp, xbuf.begin(), xbuf.capacity());
   }
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, last, boost::movelib::unantistable(comp)));
}

template<class RandIt, class Comp, class XBuf>
void initialize_keys( RandIt first, RandIt last
                    , Comp comp
                    , XBuf & xbuf)
{
   unstable_sort(first, last, comp, xbuf);
   BOOST_ASSERT(boost::movelib::is_sorted_and_unique(first, last, comp));
}

template<class RandIt, class U>
void initialize_keys( RandIt first, RandIt last
                    , less
                    , U &)
{
   typedef typename iterator_traits<RandIt>::value_type value_type;
   std::size_t count = std::size_t(last - first);
   for(std::size_t i = 0; i != count; ++i){
      *first = static_cast<value_type>(i);
      ++first;
   }
}

template <class Unsigned>
Unsigned calculate_total_combined(Unsigned const len, Unsigned const l_prev_merged, Unsigned *pl_irreg_combined = 0)
{
   typedef Unsigned size_type;

   size_type const l_combined = 2*l_prev_merged;
   size_type l_irreg_combined = len%l_combined;
   size_type l_total_combined = len;
   if(l_irreg_combined <= l_prev_merged){
      l_total_combined -= l_irreg_combined;
      l_irreg_combined = 0;
   }
   if(pl_irreg_combined)
      *pl_irreg_combined = l_irreg_combined;
   return l_total_combined;
}

template<class RandItKeys, class KeyCompare, class SizeType, class XBuf>
void combine_params
   ( RandItKeys const keys
   , KeyCompare key_comp
   , SizeType l_combined
   , SizeType const l_prev_merged
   , SizeType const l_block
   , XBuf & xbuf
   //Output
   , SizeType &n_block_a
   , SizeType &n_block_b
   , SizeType &l_irreg1
   , SizeType &l_irreg2
   //Options
   , bool do_initialize_keys = true)
{
   typedef SizeType   size_type;

   //Initial parameters for selection sort blocks
   l_irreg1 = l_prev_merged%l_block;
   l_irreg2 = (l_combined-l_irreg1)%l_block;
   BOOST_ASSERT(((l_combined-l_irreg1-l_irreg2)%l_block) == 0);
   size_type const n_reg_block = (l_combined-l_irreg1-l_irreg2)/l_block;
   n_block_a = l_prev_merged/l_block;
   n_block_b = n_reg_block - n_block_a;
   BOOST_ASSERT(n_reg_block>=n_block_a);

   //Key initialization
   if (do_initialize_keys) {
      initialize_keys(keys, keys + needed_keys_count(n_block_a, n_block_b), key_comp, xbuf);
   }
}



//////////////////////////////////
//
//          partial_merge
//
//////////////////////////////////
template<class InputIt1, class InputIt2, class OutputIt, class Compare, class Op>
OutputIt op_partial_merge_impl
   (InputIt1 &r_first1, InputIt1 const last1, InputIt2 &r_first2, InputIt2 const last2, OutputIt d_first, Compare comp, Op op)
{
   InputIt1 first1(r_first1);
   InputIt2 first2(r_first2);
   if(first2 != last2 && last1 != first1)
   while(1){
      if(comp(*first2, *first1)) {
         op(first2++, d_first++);
         if(first2 == last2){
            break;
         }
      }
      else{
         op(first1++, d_first++);
         if(first1 == last1){
            break;
         }
      }
   }
   r_first1 = first1;
   r_first2 = first2;
   return d_first;
}

template<class InputIt1, class InputIt2, class OutputIt, class Compare, class Op>
OutputIt op_partial_merge
   (InputIt1 &r_first1, InputIt1 const last1, InputIt2 &r_first2, InputIt2 const last2, OutputIt d_first, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_impl(r_first1, last1, r_first2, last2, d_first, comp, op)
                    : op_partial_merge_impl(r_first1, last1, r_first2, last2, d_first, antistable<Compare>(comp), op);
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    op_partial_merge_and_save
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
template<class InputIt1, class InputIt2, class OutputIt, class Compare, class Op>
OutputIt op_partial_merge_and_swap_impl
   (InputIt1 &r_first1, InputIt1 const last1, InputIt2 &r_first2, InputIt2 const last2, InputIt2 &r_first_min, OutputIt d_first, Compare comp, Op op)
{
   InputIt1 first1(r_first1);
   InputIt2 first2(r_first2);
   
   if(first2 != last2 && last1 != first1) {
      InputIt2 first_min(r_first_min);
      bool non_empty_ranges = true;
      do{
         if(comp(*first_min, *first1)) {
            op(three_way_t(), first2++, first_min++, d_first++);
            non_empty_ranges = first2 != last2;
         }
         else{
            op(first1++, d_first++);
            non_empty_ranges = first1 != last1;
         }
      } while(non_empty_ranges);
      r_first_min = first_min;
      r_first1 = first1;
      r_first2 = first2;
   }
   return d_first;
}

template<class RandIt, class InputIt2, class OutputIt, class Compare, class Op>
OutputIt op_partial_merge_and_swap
   (RandIt &r_first1, RandIt const last1, InputIt2 &r_first2, InputIt2 const last2, InputIt2 &r_first_min, OutputIt d_first, Compare comp, Op op, bool is_stable)
{
   return is_stable ? op_partial_merge_and_swap_impl(r_first1, last1, r_first2, last2, r_first_min, d_first, comp, op)
                    : op_partial_merge_and_swap_impl(r_first1, last1, r_first2, last2, r_first_min, d_first, antistable<Compare>(comp), op);
}

template<class RandIt1, class RandIt2, class RandItB, class Compare, class Op>
RandItB op_buffered_partial_merge_and_swap_to_range1_and_buffer
   ( RandIt1 first1, RandIt1 const last1
   , RandIt2 &rfirst2, RandIt2 const last2, RandIt2 &rfirst_min
   , RandItB &rfirstb, Compare comp, Op op )
{
   RandItB firstb = rfirstb;
   RandItB lastb  = firstb;
   RandIt2 first2 = rfirst2;

   //Move to buffer while merging
   //Three way moves need less moves when op is swap_op so use it
   //when merging elements from range2 to the destination occupied by range1
   if(first1 != last1 && first2 != last2){
      RandIt2 first_min = rfirst_min;
      op(four_way_t(), first2++, first_min++, first1++, lastb++);

      while(first1 != last1){
         if(first2 == last2){
            lastb = op(forward_t(), first1, last1, firstb);
            break;
         }

         if(comp(*first_min, *firstb)){
            op( four_way_t(), first2++, first_min++, first1++, lastb++);
         }
         else{
            op(three_way_t(), firstb++, first1++, lastb++);
         }
      }
      rfirst2 = first2;
      rfirstb = firstb;
      rfirst_min = first_min;
   }

   return lastb;
}

template<class RandIt1, class RandIt2, class RandItB, class Compare, class Op>
RandItB op_buffered_partial_merge_to_range1_and_buffer
   ( RandIt1 first1, RandIt1 const last1
   , RandIt2 &rfirst2, RandIt2 const last2
   , RandItB &rfirstb, Compare comp, Op op )
{
   RandItB firstb = rfirstb;
   RandItB lastb  = firstb;
   RandIt2 first2 = rfirst2;

   //Move to buffer while merging
   //Three way moves need less moves when op is swap_op so use it
   //when merging elements from range2 to the destination occupied by range1
   if(first1 != last1 && first2 != last2){
      op(three_way_t(), first2++, first1++, lastb++);

      while(true){
         if(first1 == last1){
            break;
         }
         if(first2 == last2){
            lastb = op(forward_t(), first1, last1, firstb);
            break;
         }
         if (comp(*first2, *firstb)) {
            op(three_way_t(), first2++, first1++, lastb++);
         }
         else {
            op(three_way_t(), firstb++, first1++, lastb++);
         }
      }
      rfirst2 = first2;
      rfirstb = firstb;
   }

   return lastb;
}

template<class RandIt, class RandItBuf, class Compare, class Op>
RandIt op_partial_merge_and_save_impl
   ( RandIt first1, RandIt const last1, RandIt &rfirst2, RandIt last2, RandIt first_min
   , RandItBuf &buf_first1_in_out, RandItBuf &buf_last1_in_out
   , Compare comp, Op op
   )
{
   RandItBuf buf_first1 = buf_first1_in_out;
   RandItBuf buf_last1  = buf_last1_in_out;
   RandIt first2(rfirst2);

   bool const do_swap = first2 != first_min;
   if(buf_first1 == buf_last1){
      //Skip any element that does not need to be moved
      RandIt new_first1 = skip_until_merge(first1, last1, *first_min, comp);
      buf_first1 += (new_first1-first1);
      first1 = new_first1;
      buf_last1  = do_swap ? op_buffered_partial_merge_and_swap_to_range1_and_buffer(first1, last1, first2, last2, first_min, buf_first1, comp, op)
                           : op_buffered_partial_merge_to_range1_and_buffer    (first1, last1, first2, last2, buf_first1, comp, op);
      first1 = last1;
   }
   else{
      BOOST_ASSERT((last1-first1) == (buf_last1 - buf_first1));
   }

   //Now merge from buffer
   first1 = do_swap ? op_partial_merge_and_swap_impl(buf_first1, buf_last1, first2, last2, first_min, first1, comp, op)
                    : op_partial_merge_impl    (buf_first1, buf_last1, first2, last2, first1, comp, op);
   buf_first1_in_out = buf_first1;
   buf_last1_in_out  = buf_last1;
   rfirst2 = first2;
   return first1;
}

template<class RandIt, class RandItBuf, class Compare, class Op>
RandIt op_partial_merge_and_save
   ( RandIt first1, RandIt const last1, RandIt &rfirst2, RandIt last2, RandIt first_min
   , RandItBuf &buf_first1_in_out
   , RandItBuf &buf_last1_in_out
   , Compare comp
   , Op op
   , bool is_stable)
{
   return is_stable
      ? op_partial_merge_and_save_impl
         (first1, last1, rfirst2, last2, first_min, buf_first1_in_out, buf_last1_in_out, comp, op)
      : op_partial_merge_and_save_impl
         (first1, last1, rfirst2, last2, first_min, buf_first1_in_out, buf_last1_in_out, antistable<Compare>(comp), op)
      ;
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    op_merge_blocks_with_irreg
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////

template<class RandItKeys, class KeyCompare, class RandIt, class RandIt2, class OutputIt, class Compare, class Op>
OutputIt op_merge_blocks_with_irreg
   ( RandItKeys key_first
   , RandItKeys key_mid
   , KeyCompare key_comp
   , RandIt first_reg
   , RandIt2 &first_irr
   , RandIt2 const last_irr
   , OutputIt dest
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type n_block_left
   , typename iterator_traits<RandIt>::size_type min_check
   , typename iterator_traits<RandIt>::size_type max_check
   , Compare comp, bool const is_stable, Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;

   for(; n_block_left; --n_block_left, ++key_first, min_check -= min_check != 0, max_check -= max_check != 0){
      size_type next_key_idx = find_next_block(key_first, key_comp, first_reg, l_block, min_check, max_check, comp);  
      max_check = min_value<size_type>(max_value<size_type>(max_check, next_key_idx+size_type(2)), n_block_left);
      RandIt const last_reg  = first_reg + l_block;
      RandIt first_min = first_reg + next_key_idx*l_block;
      RandIt const last_min  = first_min + l_block; (void)last_min;

      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first_reg, last_reg, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!next_key_idx || boost::movelib::is_sorted(first_min, last_min, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT((!next_key_idx || !comp(*first_reg, *first_min )));

      OutputIt orig_dest = dest; (void)orig_dest;
      dest = next_key_idx ? op_partial_merge_and_swap(first_irr, last_irr, first_reg, last_reg, first_min, dest, comp, op, is_stable)
                          : op_partial_merge         (first_irr, last_irr, first_reg, last_reg, dest, comp, op, is_stable);
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(orig_dest, dest, comp));

      if(first_reg == dest){
         dest = next_key_idx ? ::boost::adl_move_swap_ranges(first_min, last_min, first_reg)
                             : last_reg;
      }
      else{
         dest = next_key_idx ? op(three_way_forward_t(), first_reg, last_reg, first_min, dest)
                             : op(forward_t(), first_reg, last_reg, dest);
      }

      RandItKeys const key_next(key_first + next_key_idx);
      swap_and_update_key(key_next, key_first, key_mid, last_reg, last_reg, first_min);

      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(orig_dest, dest, comp));
      first_reg = last_reg;
   }
   return dest;
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    op_merge_blocks_left/right
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////

template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Op>
void op_merge_blocks_left
   ( RandItKeys const key_first
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_block_a
   , typename iterator_traits<RandIt>::size_type const n_block_b
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp, Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const key_count = needed_keys_count(n_block_a, n_block_b); (void)key_count;
//   BOOST_ASSERT(n_block_a || n_block_b);
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted_and_unique(key_first, key_first + key_count, key_comp));
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_b || n_block_a == count_if_with(key_first, key_first + key_count, key_comp, key_first[n_block_a]));

   size_type n_block_b_left = n_block_b;
   size_type n_block_a_left = n_block_a;
   size_type n_block_left = n_block_b + n_block_a;
   RandItKeys key_mid(key_first + n_block_a);

   RandIt buffer = first - l_block;
   RandIt first1 = first;
   RandIt last1  = first1 + l_irreg1;
   RandIt first2 = last1;
   RandIt const irreg2 = first2 + n_block_left*l_block;
   bool is_range1_A = true;

   RandItKeys key_range2(key_first);

   ////////////////////////////////////////////////////////////////////////////
   //Process all regular blocks before the irregular B block
   ////////////////////////////////////////////////////////////////////////////
   size_type min_check = n_block_a == n_block_left ? 0u : n_block_a;
   size_type max_check = min_value<size_type>(min_check+size_type(1), n_block_left);
   for (; n_block_left; --n_block_left, ++key_range2, min_check -= min_check != 0, max_check -= max_check != 0) {
      size_type const next_key_idx = find_next_block(key_range2, key_comp, first2, l_block, min_check, max_check, comp);
      max_check = min_value<size_type>(max_value<size_type>(max_check, next_key_idx+size_type(2)), n_block_left);
      RandIt const first_min = first2 + next_key_idx*l_block;
      RandIt const last_min  = first_min + l_block; (void)last_min;
      RandIt const last2  = first2 + l_block;

      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first1, last1, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first2, last2, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_left || boost::movelib::is_sorted(first_min, last_min, comp));

      //Check if irregular b block should go here.
      //If so, break to the special code handling the irregular block
      if (!n_block_b_left &&
            ( (l_irreg2 && comp(*irreg2, *first_min)) || (!l_irreg2 && is_range1_A)) ){
         break;
      }

      RandItKeys const key_next(key_range2 + next_key_idx);
      bool const is_range2_A = key_mid == (key_first+key_count) || key_comp(*key_next, *key_mid);

      bool const is_buffer_middle = last1 == buffer;
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT( ( is_buffer_middle && size_type(first2-buffer) == l_block && buffer == last1) ||
                                          (!is_buffer_middle && size_type(first1-buffer) == l_block && first2 == last1));

      if(is_range1_A == is_range2_A){
         BOOST_ASSERT((first1 == last1) || !comp(*first_min, last1[-1]));
         if(!is_buffer_middle){
            buffer = op(forward_t(), first1, last1, buffer);
         }
         swap_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min);
         first1 = first2;
         last1  = last2;
      }
      else {
         RandIt unmerged;
         RandIt buf_beg;
         RandIt buf_end;
         if(is_buffer_middle){
            buf_end = buf_beg = first2 - (last1-first1);
            unmerged = op_partial_merge_and_save( first1, last1, first2, last2, first_min
                                                , buf_beg, buf_end, comp, op, is_range1_A);
         }  
         else{
            buf_beg = first1;
            buf_end = last1;
            unmerged = op_partial_merge_and_save
               (buffer, buffer+(last1-first1), first2, last2, first_min, buf_beg, buf_end, comp, op, is_range1_A);
         }
         (void)unmerged;
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first-l_block, unmerged, comp));

         swap_and_update_key( key_next, key_range2, key_mid, first2, last2
                            , last_min - size_type(last2 - first2));

         if(buf_beg != buf_end){  //range2 exhausted: is_buffer_middle for the next iteration
            first1 = buf_beg;
            last1  = buf_end;
            BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(buf_end == (last2-l_block));
            buffer = last1;
         }
         else{ //range1 exhausted: !is_buffer_middle for the next iteration
            first1 = first2;
            last1  = last2;
            buffer = first2 - l_block;
            is_range1_A = is_range2_A;
         }
      }
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT( (is_range2_A && n_block_a_left) || (!is_range2_A && n_block_b_left));
      is_range2_A ? --n_block_a_left : --n_block_b_left;
      first2 = last2;
   }

   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_b || n_block_a == count_if_with(key_first, key_range2 + n_block_left, key_comp, *key_mid));
   BOOST_ASSERT(!n_block_b_left);

   ////////////////////////////////////////////////////////////////////////////
   //Process remaining range 1 left before the irregular B block
   ////////////////////////////////////////////////////////////////////////////
   bool const is_buffer_middle = last1 == buffer;
   RandIt first_irr2 = irreg2;
   RandIt const last_irr2  = first_irr2 + l_irreg2;
   if(l_irreg2 && is_range1_A){
      if(is_buffer_middle){
         first1 = skip_until_merge(first1, last1, *first_irr2, comp);
         //Even if we copy backward, no overlapping occurs so use forward copy
         //that can be faster specially with trivial types
         RandIt const new_first1 = first2 - (last1 - first1);
         op(forward_t(), first1, last1, new_first1);
         first1 = new_first1;
         last1 = first2;
         buffer = first1 - l_block;
      }
      buffer = op_partial_merge_impl(first1, last1, first_irr2, last_irr2, buffer, comp, op);
      buffer = op(forward_t(), first1, last1, buffer);
   }
   else if(!is_buffer_middle){
      buffer = op(forward_t(), first1, last1, buffer);
   }
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first-l_block, buffer, comp));

   ////////////////////////////////////////////////////////////////////////////
   //Process irregular B block and remaining A blocks
   ////////////////////////////////////////////////////////////////////////////
   buffer = op_merge_blocks_with_irreg
      ( key_range2, key_mid, key_comp, first2, first_irr2, last_irr2
      , buffer, l_block, n_block_left, min_check, max_check, comp, false, op);
   buffer = op(forward_t(), first_irr2, last_irr2, buffer);(void)buffer;
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first-l_block, buffer, comp));
}

// first - first element to merge.
// first[-l_block, 0) - buffer (if use_buf == true)
// l_block - length of regular blocks. First nblocks are stable sorted by 1st elements and key-coded
// keys - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be combined after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_left
   ( RandItKeys const key_first
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_block_a
   , typename iterator_traits<RandIt>::size_type const n_block_b
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , bool const xbuf_used)
{
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_b || n_block_a == count_if_with(key_first, key_first + needed_keys_count(n_block_a, n_block_b), key_comp, key_first[n_block_a]));
   if(xbuf_used){
      op_merge_blocks_left
         (key_first, key_comp, first, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp, move_op());
   }
   else{
      op_merge_blocks_left
         (key_first, key_comp, first, l_block, l_irreg1, n_block_a, n_block_b, l_irreg2, comp, swap_op());
   }
}

// first - first element to merge.
// [first+l_block*(n_bef_irreg2+n_aft_irreg2)+l_irreg2, first+l_block*(n_bef_irreg2+n_aft_irreg2+1)+l_irreg2) - buffer
// l_block - length of regular blocks. First nblocks are stable sorted by 1st elements and key-coded
// keys - sequence of keys, in same order as blocks. key<midkey means stream A
// n_bef_irreg2/n_aft_irreg2 are regular blocks
// l_irreg2 is a irregular block, that is to be combined after n_bef_irreg2 blocks and before n_aft_irreg2 blocks
// If l_irreg2==0 then n_aft_irreg2==0 (no irregular blocks).
template<class RandItKeys, class KeyCompare, class RandIt, class Compare>
void merge_blocks_right
   ( RandItKeys const key_first
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const n_block_a
   , typename iterator_traits<RandIt>::size_type const n_block_b
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , bool const xbuf_used)
{
   merge_blocks_left
      ( (make_reverse_iterator)(key_first + needed_keys_count(n_block_a, n_block_b))
      , inverse<KeyCompare>(key_comp)
      , (make_reverse_iterator)(first + ((n_block_a+n_block_b)*l_block+l_irreg2))
      , l_block
      , l_irreg2
      , n_block_b
      , n_block_a
      , 0
      , inverse<Compare>(comp), xbuf_used);
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    op_merge_blocks_with_buf
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
template<class RandItKeys, class KeyCompare, class RandIt, class Compare, class Op, class RandItBuf>
void op_merge_blocks_with_buf
   ( RandItKeys key_first
   , KeyCompare key_comp
   , RandIt const first
   , typename iterator_traits<RandIt>::size_type const l_block
   , typename iterator_traits<RandIt>::size_type const l_irreg1
   , typename iterator_traits<RandIt>::size_type const n_block_a
   , typename iterator_traits<RandIt>::size_type const n_block_b
   , typename iterator_traits<RandIt>::size_type const l_irreg2
   , Compare comp
   , Op op
   , RandItBuf const buf_first)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const key_count = needed_keys_count(n_block_a, n_block_b); (void)key_count;
   //BOOST_ASSERT(n_block_a || n_block_b);
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted_and_unique(key_first, key_first + key_count, key_comp));
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_b || n_block_a == count_if_with(key_first, key_first + key_count, key_comp, key_first[n_block_a]));

   size_type n_block_b_left = n_block_b;
   size_type n_block_a_left = n_block_a;
   size_type n_block_left = n_block_b + n_block_a;
   RandItKeys key_mid(key_first + n_block_a);

   RandItBuf buffer = buf_first;
   RandItBuf buffer_end = buffer;
   RandIt first1 = first;
   RandIt last1  = first1 + l_irreg1;
   RandIt first2 = last1;
   RandIt const first_irr2 = first2 + n_block_left*l_block;
   bool is_range1_A = true;
   const size_type len = l_block * n_block_a + l_block * n_block_b + l_irreg1 + l_irreg2; (void)len;

   RandItKeys key_range2(key_first);

   ////////////////////////////////////////////////////////////////////////////
   //Process all regular blocks before the irregular B block
   ////////////////////////////////////////////////////////////////////////////
   size_type min_check = n_block_a == n_block_left ? 0u : n_block_a;
   size_type max_check = min_value<size_type>(min_check+size_type(1), n_block_left);
   for (; n_block_left; --n_block_left, ++key_range2, min_check -= min_check != 0, max_check -= max_check != 0) {
      size_type const next_key_idx = find_next_block(key_range2, key_comp, first2, l_block, min_check, max_check, comp);
      max_check = min_value<size_type>(max_value<size_type>(max_check, next_key_idx+size_type(2)), n_block_left);
      RandIt       first_min = first2 + next_key_idx*l_block;
      RandIt const last_min  = first_min + l_block; (void)last_min;
      RandIt const last2  = first2 + l_block;

      bool const buffer_empty = buffer == buffer_end; (void)buffer_empty;
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(buffer_empty ? boost::movelib::is_sorted(first1, last1, comp) : boost::movelib::is_sorted(buffer, buffer_end, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first2, last2, comp));
      BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!n_block_left || boost::movelib::is_sorted(first_min, last_min, comp));

      //Check if irregular b block should go here.
      //If so, break to the special code handling the irregular block
      if (!n_block_b_left &&
            ( (l_irreg2 && comp(*first_irr2, *first_min)) || (!l_irreg2 && is_range1_A)) ){
         break;
      }

      RandItKeys const key_next(key_range2 + next_key_idx);
      bool const is_range2_A = key_mid == (key_first+key_count) || key_comp(*key_next, *key_mid);

      if(is_range1_A == is_range2_A){
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT((first1 == last1) || (buffer_empty ? !comp(*first_min, last1[-1]) : !comp(*first_min, buffer_end[-1])));
         //If buffered, put those elements in place
         RandIt res = op(forward_t(), buffer, buffer_end, first1);
         BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_fwd: ", len);
         buffer    = buffer_end = buf_first;
         BOOST_ASSERT(buffer_empty || res == last1); (void)res;
         //swap_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min);
         buffer_end = buffer_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min, buffer = buf_first, op);
         BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_swp: ", len);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first2, last2, comp));
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first_min, last_min, comp));
         first1 = first2;
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, first1, comp));
      }
      else {
         RandIt const unmerged = op_partial_merge_and_save(first1, last1, first2, last2, first_min, buffer, buffer_end, comp, op, is_range1_A);
         BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_mrs: ", len);
         bool const is_range_1_empty = buffer == buffer_end;
         BOOST_ASSERT(is_range_1_empty || (buffer_end-buffer) == (last1+l_block-unmerged));
         if(is_range_1_empty){
            buffer    = buffer_end = buf_first;
            first_min = last_min - (last2 - first2);
            //swap_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min);
            buffer_end = buffer_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min, buf_first, op);
         }
         else{
            first_min = last_min;
            //swap_and_update_key(key_next, key_range2, key_mid, first2, last2, first_min);
            update_key(key_next, key_range2, key_mid);
         }
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(!is_range_1_empty || (last_min-first_min) == (last2-unmerged));
         BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_swp: ", len);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first_min, last_min, comp));
         is_range1_A ^= is_range_1_empty;
         first1 = unmerged;
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, unmerged, comp));
      }
      BOOST_ASSERT( (is_range2_A && n_block_a_left) || (!is_range2_A && n_block_b_left));
      is_range2_A ? --n_block_a_left : --n_block_b_left;
      last1 += l_block;
      first2 = last2;
   }
   RandIt res = op(forward_t(), buffer, buffer_end, first1); (void)res;
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, res, comp));
   BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_fwd: ", len);

   ////////////////////////////////////////////////////////////////////////////
   //Process irregular B block and remaining A blocks
   ////////////////////////////////////////////////////////////////////////////
   RandIt const last_irr2 = first_irr2 + l_irreg2;
   op(forward_t(), first_irr2, first_irr2+l_irreg2, buf_first);
   BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_fwir:", len);
   buffer = buf_first;
   buffer_end = buffer+l_irreg2;

   reverse_iterator<RandItBuf> rbuf_beg(buffer_end);
   RandIt dest = op_merge_blocks_with_irreg
      ((make_reverse_iterator)(key_first + n_block_b + n_block_a), (make_reverse_iterator)(key_mid), inverse<KeyCompare>(key_comp)
      , (make_reverse_iterator)(first_irr2), rbuf_beg, (make_reverse_iterator)(buffer), (make_reverse_iterator)(last_irr2)
      , l_block, n_block_left, 0, n_block_left
      , inverse<Compare>(comp), true, op).base();
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(dest, last_irr2, comp));
   BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_blocks_w_irg: ", len);

   buffer_end = rbuf_beg.base();
   BOOST_ASSERT((dest-last1) == (buffer_end-buffer));
   op_merge_with_left_placed(is_range1_A ? first1 : last1, last1, dest, buffer, buffer_end, comp, op);
   BOOST_MOVE_ADAPTIVE_SORT_PRINT_L2("   merge_with_left_plc:", len);
   BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(first, last_irr2, comp));
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//  op_insertion_sort_step_left/right
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////

template<class RandIt, class Compare, class Op>
typename iterator_traits<RandIt>::size_type
   op_insertion_sort_step_left
      ( RandIt const first
      , typename iterator_traits<RandIt>::size_type const length
      , typename iterator_traits<RandIt>::size_type const step
      , Compare comp, Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const s = min_value<size_type>(step, AdaptiveSortInsertionSortThreshold);
   size_type m = 0;

   while((length - m) > s){
      insertion_sort_op(first+m, first+m+s, first+m-s, comp, op);
      m += s;
   }
   insertion_sort_op(first+m, first+length, first+m-s, comp, op);
   return s;
}

template<class RandIt, class Compare, class Op>
void op_merge_right_step_once
      ( RandIt first_block
      , typename iterator_traits<RandIt>::size_type const elements_in_blocks
      , typename iterator_traits<RandIt>::size_type const l_build_buf
      , Compare comp
      , Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type restk = elements_in_blocks%(2*l_build_buf);
   size_type p = elements_in_blocks - restk;
   BOOST_ASSERT(0 == (p%(2*l_build_buf)));

   if(restk <= l_build_buf){
      op(backward_t(),first_block+p, first_block+p+restk, first_block+p+restk+l_build_buf);
   }
   else{
      op_merge_right(first_block+p, first_block+p+l_build_buf, first_block+p+restk, first_block+p+restk+l_build_buf, comp, op);
   }
   while(p>0){
      p -= 2*l_build_buf;
      op_merge_right(first_block+p, first_block+p+l_build_buf, first_block+p+2*l_build_buf, first_block+p+3*l_build_buf, comp, op);
   }
}


//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    insertion_sort_step
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
template<class RandIt, class Compare>
typename iterator_traits<RandIt>::size_type
   insertion_sort_step
      ( RandIt const first
      , typename iterator_traits<RandIt>::size_type const length
      , typename iterator_traits<RandIt>::size_type const step
      , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   size_type const s = min_value<size_type>(step, AdaptiveSortInsertionSortThreshold);
   size_type m = 0;

   while((length - m) > s){
      insertion_sort(first+m, first+m+s, comp);
      m += s;
   }
   insertion_sort(first+m, first+length, comp);
   return s;
}

//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
//
//    op_merge_left_step_multiple
//
//////////////////////////////////
//////////////////////////////////
//////////////////////////////////
template<class RandIt, class Compare, class Op>
typename iterator_traits<RandIt>::size_type  
   op_merge_left_step_multiple
      ( RandIt first_block
      , typename iterator_traits<RandIt>::size_type const elements_in_blocks
      , typename iterator_traits<RandIt>::size_type l_merged
      , typename iterator_traits<RandIt>::size_type const l_build_buf
      , typename iterator_traits<RandIt>::size_type l_left_space
      , Compare comp
      , Op op)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   for(; l_merged < l_build_buf && l_left_space >= l_merged; l_merged*=2){
      size_type p0=0;
      RandIt pos = first_block;
      while((elements_in_blocks - p0) > 2*l_merged) {
         op_merge_left(pos-l_merged, pos, pos+l_merged, pos+2*l_merged, comp, op);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(pos-l_merged, pos+l_merged, comp));
         p0 += 2*l_merged;
         pos = first_block+p0;
      }
      if((elements_in_blocks-p0) > l_merged) {
         op_merge_left(pos-l_merged, pos, pos+l_merged, first_block+elements_in_blocks, comp, op);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(pos-l_merged, pos-l_merged+(first_block+elements_in_blocks-pos), comp));
      }
      else {
         op(forward_t(), pos, first_block+elements_in_blocks, pos-l_merged);
         BOOST_MOVE_ADAPTIVE_SORT_INVARIANT(boost::movelib::is_sorted(pos-l_merged, first_block+elements_in_blocks-l_merged, comp));
      }
      first_block -= l_merged;
      l_left_space -= l_merged;
   }
   return l_merged;
}


}  //namespace detail_adaptive {
}  //namespace movelib {
}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif   //#define BOOST_MOVE_ADAPTIVE_SORT_MERGE_HPP
