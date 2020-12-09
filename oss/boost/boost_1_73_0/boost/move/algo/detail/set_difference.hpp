//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2017-2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_SET_DIFFERENCE_HPP
#define BOOST_MOVE_SET_DIFFERENCE_HPP

#include <boost/move/algo/move.hpp>
#include <boost/move/iterator.hpp>
#include <boost/move/utility_core.hpp>

namespace boost {

namespace move_detail{

template<class InputIt, class OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt result)
{
   while (first != last) {
      *result++ = *first;
      ++result;
      ++first;
   }
   return result;
}

}  //namespace move_detail{

namespace movelib {

//Moves the elements from the sorted range [first1, last1) which are not found in the sorted
//range [first2, last2) to the range beginning at result.
//The resulting range is also sorted. Equivalent elements are treated individually,
//that is, if some element is found m times in [first1, last1) and n times in [first2, last2),
//it will be moved to result exactly max(m-n, 0) times.
//The resulting range cannot overlap with either of the input ranges.
template<class InputIt1, class InputIt2,
         class OutputIt, class Compare>
OutputIt set_difference
   (InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, OutputIt result, Compare comp)
{
   while (first1 != last1) {
      if (first2 == last2)
         return boost::move_detail::copy(first1, last1, result);

      if (comp(*first1, *first2)) {
         *result = *first1;
         ++result;
         ++first1;
      }
      else {
         if (!comp(*first2, *first1)) {
            ++first1;
         }
         ++first2;
      }
   }
   return result;
}

//Moves the elements from the sorted range [first1, last1) which are not found in the sorted
//range [first2, last2) to the range beginning at first1 (in place operation in range1).
//The resulting range is also sorted. Equivalent elements are treated individually,
//that is, if some element is found m times in [first1, last1) and n times in [first2, last2),
//it will be moved to result exactly max(m-n, 0) times.
template<class InputOutputIt1, class InputIt2, class Compare>
InputOutputIt1 inplace_set_difference
   (InputOutputIt1 first1, InputOutputIt1 last1, InputIt2 first2, InputIt2 last2, Compare comp )
{
   while (first1 != last1) {
      //Skip copying from range 1 if no element has to be skipped
      if (first2 == last2){
         return last1;
      }
      else if (comp(*first1, *first2)){
         ++first1;
      }
      else{
         if (!comp(*first2, *first1)) {
            InputOutputIt1 result = first1;
            //An element from range 1 must be skipped, no longer an inplace operation
            return boost::movelib::set_difference
               ( boost::make_move_iterator(++first1)
               , boost::make_move_iterator(last1)
               , ++first2, last2, result, comp);
         }
         ++first2;
      }
   }
   return first1;
}

//Moves the elements from the sorted range [first1, last1) which are not found in the sorted
//range [first2, last2) to the range beginning at first1.
//The resulting range is also sorted. Equivalent elements from range 1 are moved past to end
//of the result,
//that is, if some element is found m times in [first1, last1) and n times in [first2, last2),
//it will be moved to result exactly max(m-n, 0) times.
//The resulting range cannot overlap with either of the input ranges.
template<class ForwardIt1, class InputIt2,
         class OutputIt, class Compare>
OutputIt set_unique_difference
   (ForwardIt1 first1, ForwardIt1 last1, InputIt2 first2, InputIt2 last2, OutputIt result, Compare comp)
{
   while (first1 != last1) {
      if (first2 == last2){
         //unique_copy-like sequence with forward iterators but don't write i
         //to result before comparing as moving *i could alter the value in i.
         ForwardIt1 i = first1;
         while (++first1 != last1) {
            if (comp(*i, *first1)) {
               *result = *i;
               ++result;
               i = first1;
            }
         }
         *result = *i;
         ++result;
         break;
      }

      if (comp(*first1, *first2)) {
         //Skip equivalent elements in range1 but don't write i
         //to result before comparing as moving *i could alter the value in i.
         ForwardIt1 i = first1;
         while (++first1 != last1) {
            if (comp(*i, *first1)) {
               break;
            }
         }
         *result = *i;
         ++result;
      }
      else {
         if (comp(*first2, *first1)) {
            ++first2;
         }
         else{
            ++first1;
         }
      }
   }
   return result;
}

//Moves the elements from the sorted range [first1, last1) which are not found in the sorted
//range [first2, last2) to the range beginning at first1 (in place operation in range1).
//The resulting range is also sorted. Equivalent elements are treated individually,
//that is, if some element is found m times in [first1, last1) and n times in [first2, last2),
//it will be moved to result exactly max(m-n, 0) times.
template<class ForwardOutputIt1, class ForwardIt2, class Compare>
ForwardOutputIt1 inplace_set_unique_difference
   (ForwardOutputIt1 first1, ForwardOutputIt1 last1, ForwardIt2 first2, ForwardIt2 last2, Compare comp )
{
   while (first1 != last1) {
      //Skip copying from range 1 if no element has to be skipped
      if (first2 == last2){
         //unique-like algorithm for the remaining range 1
         ForwardOutputIt1 result = first1;
         while (++first1 != last1) {
            if (comp(*result, *first1) && ++result != first1) {
               *result = boost::move(*first1);
            }
         }
         return ++result;
      }
      else if (comp(*first2, *first1)) {
         ++first2;
      }
      else if (comp(*first1, *first2)){
         //skip any adjacent equivalent element in range 1
         ForwardOutputIt1 result = first1;
         if (++first1 != last1 && !comp(*result, *first1)) {
            //Some elements from range 1 must be skipped, no longer an inplace operation
            while (++first1 != last1 && !comp(*result, *first1)){}
            return boost::movelib::set_unique_difference
               ( boost::make_move_iterator(first1)
               , boost::make_move_iterator(last1)
               , first2, last2, ++result, comp);
         }
      }
      else{
         ForwardOutputIt1 result = first1;
         //Some elements from range 1 must be skipped, no longer an inplace operation
         while (++first1 != last1 && !comp(*result, *first1)){}
         //An element from range 1 must be skipped, no longer an inplace operation
         return boost::movelib::set_unique_difference
            ( boost::make_move_iterator(first1)
            , boost::make_move_iterator(last1)
            , first2, last2, result, comp);
      }
   }
   return first1;
}



}  //namespace movelib {
}  //namespace boost {

#endif   //#define BOOST_MOVE_SET_DIFFERENCE_HPP
