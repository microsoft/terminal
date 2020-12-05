//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Orson Peters  2017.
// (C) Copyright Ion Gaztanaga 2017-2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
// This implementation of Pattern-defeating quicksort (pdqsort) was written
// by Orson Peters, and discussed in the Boost mailing list:
// http://boost.2283326.n4.nabble.com/sort-pdqsort-td4691031.html
//
// This implementation is the adaptation by Ion Gaztanaga of code originally in GitHub
// with permission from the author to relicense it under the Boost Software License
// (see the Boost mailing list for details).
//
// The original copyright statement is pasted here for completeness:
//
//  pdqsort.h - Pattern-defeating quicksort.
//  Copyright (c) 2015 Orson Peters
//  This software is provided 'as-is', without any express or implied warranty. In no event will the
//  authors be held liable for any damages arising from the use of this software.
//  Permission is granted to anyone to use this software for any purpose, including commercial
//  applications, and to alter it and redistribute it freely, subject to the following restrictions:
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote the
//     original software. If you use this software in a product, an acknowledgment in the product
//     documentation would be appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//  3. This notice may not be removed or altered from any source distribution.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_MOVE_ALGO_PDQSORT_HPP
#define BOOST_MOVE_ALGO_PDQSORT_HPP

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
#include <boost/move/algo/detail/insertion_sort.hpp>
#include <boost/move/algo/detail/heap_sort.hpp>
#include <boost/move/detail/iterator_traits.hpp>

#include <boost/move/adl_move_swap.hpp>
#include <cstddef>

namespace boost {
namespace movelib {

namespace pdqsort_detail {

   //A simple pair implementation to avoid including <utility>
   template<class T1, class T2>
   struct pair
   {
      pair()
      {}

      pair(const T1 &t1, const T2 &t2)
         : first(t1), second(t2)
      {}

      T1 first;
      T2 second;
   };

    enum {
        // Partitions below this size are sorted using insertion sort.
        insertion_sort_threshold = 24,

        // Partitions above this size use Tukey's ninther to select the pivot.
        ninther_threshold = 128,

        // When we detect an already sorted partition, attempt an insertion sort that allows this
        // amount of element moves before giving up.
        partial_insertion_sort_limit = 8,

        // Must be multiple of 8 due to loop unrolling, and < 256 to fit in unsigned char.
        block_size = 64,

        // Cacheline size, assumes power of two.
        cacheline_size = 64

    };

    // Returns floor(log2(n)), assumes n > 0.
    template<class Unsigned>
    Unsigned log2(Unsigned n) {
        Unsigned log = 0;
        while (n >>= 1) ++log;
        return log;
    }

    // Attempts to use insertion sort on [begin, end). Will return false if more than
    // partial_insertion_sort_limit elements were moved, and abort sorting. Otherwise it will
    // successfully sort and return true.
    template<class Iter, class Compare>
    inline bool partial_insertion_sort(Iter begin, Iter end, Compare comp) {
        typedef typename boost::movelib::iterator_traits<Iter>::value_type T;
        typedef typename boost::movelib::iterator_traits<Iter>::size_type  size_type;
        if (begin == end) return true;
        
        size_type limit = 0;
        for (Iter cur = begin + 1; cur != end; ++cur) {
            if (limit > partial_insertion_sort_limit) return false;

            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = boost::move(*sift);

                do { *sift-- = boost::move(*sift_1); }
                while (sift != begin && comp(tmp, *--sift_1));

                *sift = boost::move(tmp);
                limit += size_type(cur - sift);
            }
        }

        return true;
    }

    template<class Iter, class Compare>
    inline void sort2(Iter a, Iter b, Compare comp) {
        if (comp(*b, *a)) boost::adl_move_iter_swap(a, b);
    }

    // Sorts the elements *a, *b and *c using comparison function comp.
    template<class Iter, class Compare>
    inline void sort3(Iter a, Iter b, Iter c, Compare comp) {
        sort2(a, b, comp);
        sort2(b, c, comp);
        sort2(a, b, comp);
    }

    // Partitions [begin, end) around pivot *begin using comparison function comp. Elements equal
    // to the pivot are put in the right-hand partition. Returns the position of the pivot after
    // partitioning and whether the passed sequence already was correctly partitioned. Assumes the
    // pivot is a median of at least 3 elements and that [begin, end) is at least
    // insertion_sort_threshold long.
    template<class Iter, class Compare>
    pdqsort_detail::pair<Iter, bool> partition_right(Iter begin, Iter end, Compare comp) {
        typedef typename boost::movelib::iterator_traits<Iter>::value_type T;
        
        // Move pivot into local for speed.
        T pivot(boost::move(*begin));

        Iter first = begin;
        Iter last = end;

        // Find the first element greater than or equal than the pivot (the median of 3 guarantees
        // this exists).
        while (comp(*++first, pivot));

        // Find the first element strictly smaller than the pivot. We have to guard this search if
        // there was no element before *first.
        if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
        else                    while (                !comp(*--last, pivot));

        // If the first pair of elements that should be swapped to partition are the same element,
        // the passed in sequence already was correctly partitioned.
        bool already_partitioned = first >= last;
        
        // Keep swapping pairs of elements that are on the wrong side of the pivot. Previously
        // swapped pairs guard the searches, which is why the first iteration is special-cased
        // above.
        while (first < last) {
            boost::adl_move_iter_swap(first, last);
            while (comp(*++first, pivot));
            while (!comp(*--last, pivot));
        }

        // Put the pivot in the right place.
        Iter pivot_pos = first - 1;
        *begin = boost::move(*pivot_pos);
        *pivot_pos = boost::move(pivot);

        return pdqsort_detail::pair<Iter, bool>(pivot_pos, already_partitioned);
    }

    // Similar function to the one above, except elements equal to the pivot are put to the left of
    // the pivot and it doesn't check or return if the passed sequence already was partitioned.
    // Since this is rarely used (the many equal case), and in that case pdqsort already has O(n)
    // performance, no block quicksort is applied here for simplicity.
    template<class Iter, class Compare>
    inline Iter partition_left(Iter begin, Iter end, Compare comp) {
        typedef typename boost::movelib::iterator_traits<Iter>::value_type T;

        T pivot(boost::move(*begin));
        Iter first = begin;
        Iter last = end;
        
        while (comp(pivot, *--last));

        if (last + 1 == end) while (first < last && !comp(pivot, *++first));
        else                 while (                !comp(pivot, *++first));

        while (first < last) {
            boost::adl_move_iter_swap(first, last);
            while (comp(pivot, *--last));
            while (!comp(pivot, *++first));
        }

        Iter pivot_pos = last;
        *begin = boost::move(*pivot_pos);
        *pivot_pos = boost::move(pivot);

        return pivot_pos;
    }


   template<class Iter, class Compare>
   void pdqsort_loop( Iter begin, Iter end, Compare comp
                    , typename boost::movelib::iterator_traits<Iter>::size_type bad_allowed
                    , bool leftmost = true)
   {
        typedef typename boost::movelib::iterator_traits<Iter>::size_type size_type;

        // Use a while loop for tail recursion elimination.
        while (true) {
            size_type size = size_type(end - begin);

            // Insertion sort is faster for small arrays.
            if (size < insertion_sort_threshold) {
                insertion_sort(begin, end, comp);
                return;
            }

            // Choose pivot as median of 3 or pseudomedian of 9.
            size_type s2 = size / 2;
            if (size > ninther_threshold) {
                sort3(begin, begin + s2, end - 1, comp);
                sort3(begin + 1, begin + (s2 - 1), end - 2, comp);
                sort3(begin + 2, begin + (s2 + 1), end - 3, comp);
                sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), comp);
                boost::adl_move_iter_swap(begin, begin + s2);
            } else sort3(begin + s2, begin, end - 1, comp);

            // If *(begin - 1) is the end of the right partition of a previous partition operation
            // there is no element in [begin, end) that is smaller than *(begin - 1). Then if our
            // pivot compares equal to *(begin - 1) we change strategy, putting equal elements in
            // the left partition, greater elements in the right partition. We do not have to
            // recurse on the left partition, since it's sorted (all equal).
            if (!leftmost && !comp(*(begin - 1), *begin)) {
                begin = partition_left(begin, end, comp) + 1;
                continue;
            }

            // Partition and get results.
            pdqsort_detail::pair<Iter, bool> part_result = partition_right(begin, end, comp);
            Iter pivot_pos = part_result.first;
            bool already_partitioned = part_result.second;

            // Check for a highly unbalanced partition.
            size_type l_size = size_type(pivot_pos - begin);
            size_type r_size = size_type(end - (pivot_pos + 1));
            bool highly_unbalanced = l_size < size / 8 || r_size < size / 8;

            // If we got a highly unbalanced partition we shuffle elements to break many patterns.
            if (highly_unbalanced) {
                // If we had too many bad partitions, switch to heapsort to guarantee O(n log n).
                if (--bad_allowed == 0) {
                    boost::movelib::heap_sort(begin, end, comp);
                    return;
                }

                if (l_size >= insertion_sort_threshold) {
                    boost::adl_move_iter_swap(begin,             begin + l_size / 4);
                    boost::adl_move_iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);

                    if (l_size > ninther_threshold) {
                        boost::adl_move_iter_swap(begin + 1,         begin + (l_size / 4 + 1));
                        boost::adl_move_iter_swap(begin + 2,         begin + (l_size / 4 + 2));
                        boost::adl_move_iter_swap(pivot_pos - 2, pivot_pos - (l_size / 4 + 1));
                        boost::adl_move_iter_swap(pivot_pos - 3, pivot_pos - (l_size / 4 + 2));
                    }
                }
                
                if (r_size >= insertion_sort_threshold) {
                    boost::adl_move_iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
                    boost::adl_move_iter_swap(end - 1,                   end - r_size / 4);
                    
                    if (r_size > ninther_threshold) {
                        boost::adl_move_iter_swap(pivot_pos + 2, pivot_pos + (2 + r_size / 4));
                        boost::adl_move_iter_swap(pivot_pos + 3, pivot_pos + (3 + r_size / 4));
                        boost::adl_move_iter_swap(end - 2,             end - (1 + r_size / 4));
                        boost::adl_move_iter_swap(end - 3,             end - (2 + r_size / 4));
                    }
                }
            } else {
                // If we were decently balanced and we tried to sort an already partitioned
                // sequence try to use insertion sort.
                if (already_partitioned && partial_insertion_sort(begin, pivot_pos, comp)
                                        && partial_insertion_sort(pivot_pos + 1, end, comp)) return;
            }
                
            // Sort the left partition first using recursion and do tail recursion elimination for
            // the right-hand partition.
            pdqsort_loop<Iter, Compare>(begin, pivot_pos, comp, bad_allowed, leftmost);
            begin = pivot_pos + 1;
            leftmost = false;
        }
    }
}


template<class Iter, class Compare>
void pdqsort(Iter begin, Iter end, Compare comp)
{
   if (begin == end) return;
   typedef typename boost::movelib::iterator_traits<Iter>::size_type size_type;
   pdqsort_detail::pdqsort_loop<Iter, Compare>(begin, end, comp, pdqsort_detail::log2(size_type(end - begin)));
}

}  //namespace movelib {
}  //namespace boost {

#include <boost/move/detail/config_end.hpp>

#endif   //BOOST_MOVE_ALGO_PDQSORT_HPP
