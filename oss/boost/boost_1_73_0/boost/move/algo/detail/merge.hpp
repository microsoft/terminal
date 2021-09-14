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
#ifndef BOOST_MOVE_MERGE_HPP
#define BOOST_MOVE_MERGE_HPP

#include <boost/move/algo/move.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/algo/detail/basic_op.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <boost/move/detail/destruct_n.hpp>
#include <boost/move/algo/predicate.hpp>
#include <boost/move/detail/iterator_to_raw_pointer.hpp>
#include <boost/assert.hpp>
#include <cstddef>

namespace boost {
namespace movelib {

template<class T, class RandRawIt = T*, class SizeType = typename iterator_traits<RandRawIt>::size_type>
class adaptive_xbuf
{
   adaptive_xbuf(const adaptive_xbuf &);
   adaptive_xbuf & operator=(const adaptive_xbuf &);

   #if !defined(UINTPTR_MAX)
   typedef std::size_t uintptr_t;
   #endif

   public:
   typedef RandRawIt iterator;
   typedef SizeType  size_type;

   adaptive_xbuf()
      : m_ptr(), m_size(0), m_capacity(0)
   {}

   adaptive_xbuf(RandRawIt raw_memory, size_type capacity)
      : m_ptr(raw_memory), m_size(0), m_capacity(capacity)
   {}

   template<class RandIt>
   void move_assign(RandIt first, size_type n)
   {
      if(n <= m_size){
         boost::move(first, first+n, m_ptr);
         size_type size = m_size;
         while(size-- != n){
            m_ptr[size].~T();
         }
         m_size = n;
      }
      else{
         RandRawIt result = boost::move(first, first+m_size, m_ptr);
         boost::uninitialized_move(first+m_size, first+n, result);
         m_size = n;
      }
   }

   template<class RandIt>
   void push_back(RandIt first, size_type n)
   {
      BOOST_ASSERT(m_capacity - m_size >= n);
      boost::uninitialized_move(first, first+n, m_ptr+m_size);
      m_size += n;
   }

   template<class RandIt>
   iterator add(RandIt it)
   {
      BOOST_ASSERT(m_size < m_capacity);
      RandRawIt p_ret = m_ptr + m_size;
      ::new(&*p_ret) T(::boost::move(*it));
      ++m_size;
      return p_ret;
   }

   template<class RandIt>
   void insert(iterator pos, RandIt it)
   {
      if(pos == (m_ptr + m_size)){
         this->add(it);
      }
      else{
         this->add(m_ptr+m_size-1);
         //m_size updated
         boost::move_backward(pos, m_ptr+m_size-2, m_ptr+m_size-1);
         *pos = boost::move(*it);
      }
   }

   void set_size(size_type size)
   {
      m_size = size;
   }

   void shrink_to_fit(size_type const size)
   {
      if(m_size > size){
         for(size_type szt_i = size; szt_i != m_size; ++szt_i){
            m_ptr[szt_i].~T();
         }
         m_size = size;
      }
   }

   void initialize_until(size_type const size, T &t)
   {
      BOOST_ASSERT(m_size < m_capacity);
      if(m_size < size){
         BOOST_TRY
         {
            ::new((void*)&m_ptr[m_size]) T(::boost::move(t));
            ++m_size;
            for(; m_size != size; ++m_size){
               ::new((void*)&m_ptr[m_size]) T(::boost::move(m_ptr[m_size-1]));
            }
            t = ::boost::move(m_ptr[m_size-1]);
         }
         BOOST_CATCH(...)
         {
            while(m_size)
            {
               --m_size;
               m_ptr[m_size].~T();
            }
         }
         BOOST_CATCH_END
      }
   }

   private:
   template<class RIt>
   static bool is_raw_ptr(RIt)
   {
      return false;
   }

   static bool is_raw_ptr(T*)
   {
      return true;
   }

   public:
   template<class U>
   bool supports_aligned_trailing(size_type size, size_type trail_count) const
   {
      if(this->is_raw_ptr(this->data()) && m_capacity){
         uintptr_t u_addr_sz = uintptr_t(&*(this->data()+size));
         uintptr_t u_addr_cp = uintptr_t(&*(this->data()+this->capacity()));
         u_addr_sz = ((u_addr_sz + sizeof(U)-1)/sizeof(U))*sizeof(U);
         return (u_addr_cp >= u_addr_sz) && ((u_addr_cp - u_addr_sz)/sizeof(U) >= trail_count);
      }
      return false;
   }

   template<class U>
   U *aligned_trailing() const
   {
      return this->aligned_trailing<U>(this->size());
   }

   template<class U>
   U *aligned_trailing(size_type pos) const
   {
      uintptr_t u_addr = uintptr_t(&*(this->data()+pos));
      u_addr = ((u_addr + sizeof(U)-1)/sizeof(U))*sizeof(U);
      return (U*)u_addr;
   }

   ~adaptive_xbuf()
   {
      this->clear();
   }

   size_type capacity() const
   {  return m_capacity;   }

   iterator data() const
   {  return m_ptr;   }

   iterator begin() const
   {  return m_ptr;   }

   iterator end() const
   {  return m_ptr+m_size;   }

   size_type size() const
   {  return m_size;   }

   bool empty() const
   {  return !m_size;   }

   void clear()
   {
      this->shrink_to_fit(0u);
   }

   private:
   RandRawIt m_ptr;
   size_type m_size;
   size_type m_capacity;
};

template<class Iterator, class SizeType, class Op>
class range_xbuf
{
   range_xbuf(const range_xbuf &);
   range_xbuf & operator=(const range_xbuf &);

   public:
   typedef SizeType size_type;
   typedef Iterator iterator;

   range_xbuf(Iterator first, Iterator last)
      : m_first(first), m_last(first), m_cap(last)
   {}

   template<class RandIt>
   void move_assign(RandIt first, size_type n)
   {
      BOOST_ASSERT(size_type(n) <= size_type(m_cap-m_first));
      m_last = Op()(forward_t(), first, first+n, m_first);
   }

   ~range_xbuf()
   {}

   size_type capacity() const
   {  return m_cap-m_first;   }

   Iterator data() const
   {  return m_first;   }

   Iterator end() const
   {  return m_last;   }

   size_type size() const
   {  return m_last-m_first;   }

   bool empty() const
   {  return m_first == m_last;   }

   void clear()
   {
      m_last = m_first;
   }

   template<class RandIt>
   iterator add(RandIt it)
   {
      Iterator pos(m_last);
      *pos = boost::move(*it);
      ++m_last;
      return pos;
   }

   void set_size(size_type size)
   {
      m_last  = m_first;
      m_last += size;
   }

   private:
   Iterator const m_first;
   Iterator m_last;
   Iterator const m_cap;
};



// @cond

/*
template<typename Unsigned>
inline Unsigned gcd(Unsigned x, Unsigned y)
{
   if(0 == ((x &(x-1)) | (y & (y-1)))){
      return x < y ? x : y;
   }
   else{
      do
      {
         Unsigned t = x % y;
         x = y;
         y = t;
      } while (y);
      return x;
   }
}
*/

//Modified version from "An Optimal In-Place Array Rotation Algorithm", Ching-Kuang Shene
template<typename Unsigned>
Unsigned gcd(Unsigned x, Unsigned y)
{
   if(0 == ((x &(x-1)) | (y & (y-1)))){
      return x < y ? x : y;
   }
   else{
      Unsigned z = 1;
      while((!(x&1)) & (!(y&1))){
         z <<=1, x>>=1, y>>=1;
      }
      while(x && y){
         if(!(x&1))
            x >>=1;
         else if(!(y&1))
            y >>=1;
         else if(x >=y)
            x = (x-y) >> 1;
         else
            y = (y-x) >> 1;
      }
      return z*(x+y);
   }
}

template<typename RandIt>
RandIt rotate_gcd(RandIt first, RandIt middle, RandIt last)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   typedef typename iterator_traits<RandIt>::value_type value_type;

   if(first == middle)
      return last;
   if(middle == last)
      return first;
   const size_type middle_pos = size_type(middle - first);
   RandIt ret = last - middle_pos;
   if (middle == ret){
      boost::adl_move_swap_ranges(first, middle, middle);
   }
   else{
      const size_type length = size_type(last - first);
      for( RandIt it_i(first), it_gcd(it_i + gcd(length, middle_pos))
         ; it_i != it_gcd
         ; ++it_i){
         value_type temp(boost::move(*it_i));
         RandIt it_j = it_i;
         RandIt it_k = it_j+middle_pos;
         do{
            *it_j = boost::move(*it_k);
            it_j = it_k;
            size_type const left = size_type(last - it_j);
            it_k = left > middle_pos ? it_j + middle_pos : first + (middle_pos - left);
         } while(it_k != it_i);
         *it_j = boost::move(temp);
      }
   }
   return ret;
}

template <class RandIt, class T, class Compare>
RandIt lower_bound
   (RandIt first, const RandIt last, const T& key, Compare comp)
{
   typedef typename iterator_traits
      <RandIt>::size_type size_type;
   size_type len = size_type(last - first);
   RandIt middle;

   while (len) {
      size_type step = len >> 1;
      middle = first;
      middle += step;

      if (comp(*middle, key)) {
         first = ++middle;
         len -= step + 1;
      }
      else{
         len = step;
      }
   }
   return first;
}

template <class RandIt, class T, class Compare>
RandIt upper_bound
   (RandIt first, const RandIt last, const T& key, Compare comp)
{
   typedef typename iterator_traits
      <RandIt>::size_type size_type;
   size_type len = size_type(last - first);
   RandIt middle;

   while (len) {
      size_type step = len >> 1;
      middle = first;
      middle += step;

      if (!comp(key, *middle)) {
         first = ++middle;
         len -= step + 1;
      }
      else{
         len = step;
      }
   }
   return first;
}


template<class RandIt, class Compare, class Op>
void op_merge_left( RandIt buf_first
                    , RandIt first1
                    , RandIt const last1
                    , RandIt const last2
                    , Compare comp
                    , Op op)
{
   for(RandIt first2=last1; first2 != last2; ++buf_first){
      if(first1 == last1){
         op(forward_t(), first2, last2, buf_first);
         return;
      }
      else if(comp(*first2, *first1)){
         op(first2, buf_first);
         ++first2;
      }
      else{
         op(first1, buf_first);
         ++first1;
      }
   }
   if(buf_first != first1){//In case all remaining elements are in the same place
                           //(e.g. buffer is exactly the size of the second half
                           //and all elements from the second half are less)
      op(forward_t(), first1, last1, buf_first);
   }
}

// [buf_first, first1) -> buffer
// [first1, last1) merge [last1,last2) -> [buf_first,buf_first+(last2-first1))
// Elements from buffer are moved to [last2 - (first1-buf_first), last2)
// Note: distance(buf_first, first1) >= distance(last1, last2), so no overlapping occurs
template<class RandIt, class Compare>
void merge_left
   (RandIt buf_first, RandIt first1, RandIt const last1, RandIt const last2, Compare comp)
{
   op_merge_left(buf_first, first1, last1, last2, comp, move_op());
}

// [buf_first, first1) -> buffer
// [first1, last1) merge [last1,last2) -> [buf_first,buf_first+(last2-first1))
// Elements from buffer are swapped to [last2 - (first1-buf_first), last2)
// Note: distance(buf_first, first1) >= distance(last1, last2), so no overlapping occurs
template<class RandIt, class Compare>
void swap_merge_left
   (RandIt buf_first, RandIt first1, RandIt const last1, RandIt const last2, Compare comp)
{
   op_merge_left(buf_first, first1, last1, last2, comp, swap_op());
}

template<class RandIt, class Compare, class Op>
void op_merge_right
   (RandIt const first1, RandIt last1, RandIt last2, RandIt buf_last, Compare comp, Op op)
{
   RandIt const first2 = last1;
   while(first1 != last1){
      if(last2 == first2){
         op(backward_t(), first1, last1, buf_last);
         return;
      }
      --last2;
      --last1;
      --buf_last;
      if(comp(*last2, *last1)){
         op(last1, buf_last);
         ++last2;
      }
      else{
         op(last2, buf_last);
         ++last1;
      }
   }
   if(last2 != buf_last){  //In case all remaining elements are in the same place
                           //(e.g. buffer is exactly the size of the first half
                           //and all elements from the second half are less)
      op(backward_t(), first2, last2, buf_last);
   }
}

// [last2, buf_last) - buffer
// [first1, last1) merge [last1,last2) -> [first1+(buf_last-last2), buf_last)
// Note: distance[last2, buf_last) >= distance[first1, last1), so no overlapping occurs
template<class RandIt, class Compare>
void merge_right
   (RandIt first1, RandIt last1, RandIt last2, RandIt buf_last, Compare comp)
{
   op_merge_right(first1, last1, last2, buf_last, comp, move_op());
}

// [last2, buf_last) - buffer
// [first1, last1) merge [last1,last2) -> [first1+(buf_last-last2), buf_last)
// Note: distance[last2, buf_last) >= distance[first1, last1), so no overlapping occurs
template<class RandIt, class Compare>
void swap_merge_right
   (RandIt first1, RandIt last1, RandIt last2, RandIt buf_last, Compare comp)
{
   op_merge_right(first1, last1, last2, buf_last, comp, swap_op());
}

///////////////////////////////////////////////////////////////////////////////
//
//                            BUFFERED MERGE
//
///////////////////////////////////////////////////////////////////////////////
template<class RandIt, class Compare, class Op, class Buf>
void op_buffered_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp, Op op
      , Buf &xbuf)
{
   if(first != middle && middle != last && comp(*middle, middle[-1])){
      typedef typename iterator_traits<RandIt>::size_type   size_type;
      size_type const len1 = size_type(middle-first);
      size_type const len2 = size_type(last-middle);
      if(len1 <= len2){
         first = boost::movelib::upper_bound(first, middle, *middle, comp);
         xbuf.move_assign(first, size_type(middle-first));
         op_merge_with_right_placed
            (xbuf.data(), xbuf.end(), first, middle, last, comp, op);
      }
      else{
         last = boost::movelib::lower_bound(middle, last, middle[-1], comp);
         xbuf.move_assign(middle, size_type(last-middle));
         op_merge_with_left_placed
            (first, middle, last, xbuf.data(), xbuf.end(), comp, op);
      }
   }
}

template<class RandIt, class Compare, class XBuf>
void buffered_merge
      ( RandIt first, RandIt const middle, RandIt last
      , Compare comp
      , XBuf &xbuf)
{
   op_buffered_merge(first, middle, last, comp, move_op(), xbuf);
}

//Complexity: min(len1,len2)^2 + max(len1,len2)
template<class RandIt, class Compare>
void merge_bufferless_ON2(RandIt first, RandIt middle, RandIt last, Compare comp)
{
   if((middle - first) < (last - middle)){
      while(first != middle){
         RandIt const old_last1 = middle;
         middle = boost::movelib::lower_bound(middle, last, *first, comp);
         first = rotate_gcd(first, old_last1, middle);
         if(middle == last){
            break;
         }
         do{
            ++first;
         } while(first != middle && !comp(*middle, *first));
      }
   }
   else{
      while(middle != last){
         RandIt p = boost::movelib::upper_bound(first, middle, last[-1], comp);
         last = rotate_gcd(p, middle, last);
         middle = p;
         if(middle == first){
            break;
         }
         --p;
         do{
            --last;
         } while(middle != last && !comp(last[-1], *p));
      }
   }
}

static const std::size_t MergeBufferlessONLogNRotationThreshold = 16u;

template <class RandIt, class Compare>
void merge_bufferless_ONlogN_recursive
   ( RandIt first, RandIt middle, RandIt last
   , typename iterator_traits<RandIt>::size_type len1
   , typename iterator_traits<RandIt>::size_type len2
   , Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;

   while(1) {
      //trivial cases
      if (!len2) {
         return;
      }
      else if (!len1) {
         return;
      }
      else if (size_type(len1 | len2) == 1u) {
         if (comp(*middle, *first))
            adl_move_swap(*first, *middle);  
         return;
      }
      else if(size_type(len1+len2) < MergeBufferlessONLogNRotationThreshold){
         merge_bufferless_ON2(first, middle, last, comp);
         return;
      }

      RandIt first_cut = first;
      RandIt second_cut = middle;
      size_type len11 = 0;
      size_type len22 = 0;
      if (len1 > len2) {
         len11 = len1 / 2;
         first_cut +=  len11;
         second_cut = boost::movelib::lower_bound(middle, last, *first_cut, comp);
         len22 = size_type(second_cut - middle);
      }
      else {
         len22 = len2 / 2;
         second_cut += len22;
         first_cut = boost::movelib::upper_bound(first, middle, *second_cut, comp);
         len11 = size_type(first_cut - first);
      }
      RandIt new_middle = rotate_gcd(first_cut, middle, second_cut);

      //Avoid one recursive call doing a manual tail call elimination on the biggest range
      const size_type len_internal = len11+len22;
      if( len_internal < (len1 + len2 - len_internal) ) {
         merge_bufferless_ONlogN_recursive(first, first_cut,  new_middle, len11, len22,        comp);
         first = new_middle;
         middle = second_cut;
         len1 -= len11;
         len2 -= len22;
      }
      else {
         merge_bufferless_ONlogN_recursive(new_middle, second_cut, last, len1 - len11, len2 - len22, comp);
         middle = first_cut;
         last = new_middle;
         len1 = len11;
         len2 = len22;
      }
   }
}


//Complexity: NlogN
template<class RandIt, class Compare>
void merge_bufferless_ONlogN(RandIt first, RandIt middle, RandIt last, Compare comp)
{
   typedef typename iterator_traits<RandIt>::size_type size_type;
   merge_bufferless_ONlogN_recursive
      (first, middle, last, size_type(middle - first), size_type(last - middle), comp);
}

template<class RandIt, class Compare>
void merge_bufferless(RandIt first, RandIt middle, RandIt last, Compare comp)
{
   #define BOOST_ADAPTIVE_MERGE_NLOGN_MERGE
   #ifdef BOOST_ADAPTIVE_MERGE_NLOGN_MERGE
   merge_bufferless_ONlogN(first, middle, last, comp);
   #else
   merge_bufferless_ON2(first, middle, last, comp);
   #endif   //BOOST_ADAPTIVE_MERGE_NLOGN_MERGE
}

// [r_first, r_last) are already in the right part of the destination range.
template <class Compare, class InputIterator, class InputOutIterator, class Op>
void op_merge_with_right_placed
   ( InputIterator first, InputIterator last
   , InputOutIterator dest_first, InputOutIterator r_first, InputOutIterator r_last
   , Compare comp, Op op)
{
   BOOST_ASSERT((last - first) == (r_first - dest_first));
   while ( first != last ) {
      if (r_first == r_last) {
         InputOutIterator end = op(forward_t(), first, last, dest_first);
         BOOST_ASSERT(end == r_last);
         (void)end;
         return;
      }
      else if (comp(*r_first, *first)) {
         op(r_first, dest_first);
         ++r_first;
      }
      else {
         op(first, dest_first);
         ++first;
      }
      ++dest_first;
   }
   // Remaining [r_first, r_last) already in the correct place
}

template <class Compare, class InputIterator, class InputOutIterator>
void swap_merge_with_right_placed
   ( InputIterator first, InputIterator last
   , InputOutIterator dest_first, InputOutIterator r_first, InputOutIterator r_last
   , Compare comp)
{
   op_merge_with_right_placed(first, last, dest_first, r_first, r_last, comp, swap_op());
}

// [first, last) are already in the right part of the destination range.
template <class Compare, class Op, class BidirIterator, class BidirOutIterator>
void op_merge_with_left_placed
   ( BidirOutIterator const first, BidirOutIterator last, BidirOutIterator dest_last
   , BidirIterator const r_first, BidirIterator r_last
   , Compare comp, Op op)
{
   BOOST_ASSERT((dest_last - last) == (r_last - r_first));
   while( r_first != r_last ) {
      if(first == last) {
         BidirOutIterator res = op(backward_t(), r_first, r_last, dest_last);
         BOOST_ASSERT(last == res);
         (void)res;
         return;
      }
      --r_last;
      --last;
      if(comp(*r_last, *last)){
         ++r_last;
         --dest_last;
         op(last, dest_last);
      }
      else{
         ++last;
         --dest_last;
         op(r_last, dest_last);
      }
   }
   // Remaining [first, last) already in the correct place
}

// @endcond

// [first, last) are already in the right part of the destination range.
template <class Compare, class BidirIterator, class BidirOutIterator>
void merge_with_left_placed
   ( BidirOutIterator const first, BidirOutIterator last, BidirOutIterator dest_last
   , BidirIterator const r_first, BidirIterator r_last
   , Compare comp)
{
   op_merge_with_left_placed(first, last, dest_last, r_first, r_last, comp, move_op());
}

// [r_first, r_last) are already in the right part of the destination range.
template <class Compare, class InputIterator, class InputOutIterator>
void merge_with_right_placed
   ( InputIterator first, InputIterator last
   , InputOutIterator dest_first, InputOutIterator r_first, InputOutIterator r_last
   , Compare comp)
{
   op_merge_with_right_placed(first, last, dest_first, r_first, r_last, comp, move_op());
}

// [r_first, r_last) are already in the right part of the destination range.
// [dest_first, r_first) is uninitialized memory
template <class Compare, class InputIterator, class InputOutIterator>
void uninitialized_merge_with_right_placed
   ( InputIterator first, InputIterator last
   , InputOutIterator dest_first, InputOutIterator r_first, InputOutIterator r_last
   , Compare comp)
{
   BOOST_ASSERT((last - first) == (r_first - dest_first));
   typedef typename iterator_traits<InputOutIterator>::value_type value_type;
   InputOutIterator const original_r_first = r_first;

   destruct_n<value_type, InputOutIterator> d(dest_first);

   while ( first != last && dest_first != original_r_first ) {
      if (r_first == r_last) {
         for(; dest_first != original_r_first; ++dest_first, ++first){
            ::new((iterator_to_raw_pointer)(dest_first)) value_type(::boost::move(*first));
            d.incr();
         }
         d.release();
         InputOutIterator end = ::boost::move(first, last, original_r_first);
         BOOST_ASSERT(end == r_last);
         (void)end;
         return;
      }
      else if (comp(*r_first, *first)) {
         ::new((iterator_to_raw_pointer)(dest_first)) value_type(::boost::move(*r_first));
         d.incr();
         ++r_first;
      }
      else {
         ::new((iterator_to_raw_pointer)(dest_first)) value_type(::boost::move(*first));
         d.incr();
         ++first;
      }
      ++dest_first;
   }
   d.release();
   merge_with_right_placed(first, last, original_r_first, r_first, r_last, comp);
}

/*
// [r_first, r_last) are already in the right part of the destination range.
// [dest_first, r_first) is uninitialized memory
template <class Compare, class BidirOutIterator, class BidirIterator>
void uninitialized_merge_with_left_placed
   ( BidirOutIterator dest_first, BidirOutIterator r_first, BidirOutIterator r_last
   , BidirIterator first, BidirIterator last
   , Compare comp)
{
   BOOST_ASSERT((last - first) == (r_last - r_first));
   typedef typename iterator_traits<BidirOutIterator>::value_type value_type;
   BidirOutIterator const original_r_last = r_last;

   destruct_n<value_type> d(&*dest_last);

   while ( first != last && dest_first != original_r_first ) {
      if (r_first == r_last) {
         for(; dest_first != original_r_first; ++dest_first, ++first){
            ::new(&*dest_first) value_type(::boost::move(*first));
            d.incr();
         }
         d.release();
         BidirOutIterator end = ::boost::move(first, last, original_r_first);
         BOOST_ASSERT(end == r_last);
         (void)end;
         return;
      }
      else if (comp(*r_first, *first)) {
         ::new(&*dest_first) value_type(::boost::move(*r_first));
         d.incr();
         ++r_first;
      }
      else {
         ::new(&*dest_first) value_type(::boost::move(*first));
         d.incr();
         ++first;
      }
      ++dest_first;
   }
   d.release();
   merge_with_right_placed(first, last, original_r_first, r_first, r_last, comp);
}
*/


/// This is a helper function for the merge routines.
template<typename BidirectionalIterator1, typename BidirectionalIterator2>
   BidirectionalIterator1
   rotate_adaptive(BidirectionalIterator1 first,
      BidirectionalIterator1 middle,
      BidirectionalIterator1 last,
      typename iterator_traits<BidirectionalIterator1>::size_type len1,
      typename iterator_traits<BidirectionalIterator1>::size_type len2,
      BidirectionalIterator2 buffer,
      typename iterator_traits<BidirectionalIterator1>::size_type buffer_size)
{
   if (len1 > len2 && len2 <= buffer_size)
   {
      if(len2) //Protect against self-move ranges
      {
         BidirectionalIterator2 buffer_end = boost::move(middle, last, buffer);
         boost::move_backward(first, middle, last);
         return boost::move(buffer, buffer_end, first);
      }
      else
         return first;
   }
   else if (len1 <= buffer_size)
   {
      if(len1) //Protect against self-move ranges
      {
         BidirectionalIterator2 buffer_end = boost::move(first, middle, buffer);
         BidirectionalIterator1 ret = boost::move(middle, last, first);
         boost::move(buffer, buffer_end, ret);
         return ret;
      }
      else
         return last;
   }
   else
      return rotate_gcd(first, middle, last);
}

template<typename BidirectionalIterator,
   typename Pointer, typename Compare>
   void merge_adaptive_ONlogN_recursive
   (BidirectionalIterator first,
      BidirectionalIterator middle,
      BidirectionalIterator last,
      typename iterator_traits<BidirectionalIterator>::size_type len1,
      typename iterator_traits<BidirectionalIterator>::size_type len2,
      Pointer buffer,
      typename iterator_traits<BidirectionalIterator>::size_type buffer_size,
      Compare comp)
{
   typedef typename iterator_traits<BidirectionalIterator>::size_type size_type;
   //trivial cases
   if (!len2 || !len1) {
      return;
   }
   else if (len1 <= buffer_size || len2 <= buffer_size)
   {
      range_xbuf<Pointer, size_type, move_op> rxbuf(buffer, buffer + buffer_size);
      buffered_merge(first, middle, last, comp, rxbuf);
   }
   else if (size_type(len1 + len2) == 2u) {
      if (comp(*middle, *first))
         adl_move_swap(*first, *middle);
      return;
   }
   else if (size_type(len1 + len2) < MergeBufferlessONLogNRotationThreshold) {
      merge_bufferless_ON2(first, middle, last, comp);
      return;
   }
   BidirectionalIterator first_cut = first;
   BidirectionalIterator second_cut = middle;
   size_type len11 = 0;
   size_type len22 = 0;
   if (len1 > len2)  //(len1 < len2)
   {
      len11 = len1 / 2;
      first_cut += len11;
      second_cut = boost::movelib::lower_bound(middle, last, *first_cut, comp);
      len22 = second_cut - middle;
   }
   else
   {
      len22 = len2 / 2;
      second_cut += len22;
      first_cut = boost::movelib::upper_bound(first, middle, *second_cut, comp);
      len11 = first_cut - first;
   }

   BidirectionalIterator new_middle
      = rotate_adaptive(first_cut, middle, second_cut,
         size_type(len1 - len11), len22, buffer,
         buffer_size);
   merge_adaptive_ONlogN_recursive(first, first_cut, new_middle, len11,
      len22, buffer, buffer_size, comp);
   merge_adaptive_ONlogN_recursive(new_middle, second_cut, last,
      len1 - len11, len2 - len22, buffer, buffer_size, comp);
}


template<typename BidirectionalIterator, typename Compare, typename RandRawIt>
void merge_adaptive_ONlogN(BidirectionalIterator first,
		                     BidirectionalIterator middle,
		                     BidirectionalIterator last,
		                     Compare comp,
                           RandRawIt uninitialized,
                           typename iterator_traits<BidirectionalIterator>::size_type uninitialized_len)
{
   typedef typename iterator_traits<BidirectionalIterator>::value_type  value_type;
   typedef typename iterator_traits<BidirectionalIterator>::size_type   size_type;

   if (first == middle || middle == last)
      return;

   if(uninitialized_len)
   {
      const size_type len1 = size_type(middle - first);
      const size_type len2 = size_type(last - middle);

      ::boost::movelib::adaptive_xbuf<value_type, RandRawIt> xbuf(uninitialized, uninitialized_len);
      xbuf.initialize_until(uninitialized_len, *first);
	   merge_adaptive_ONlogN_recursive(first, middle, last, len1, len2, xbuf.begin(), uninitialized_len, comp);
   }
   else
   {
      merge_bufferless(first, middle, last, comp);
   }
}


}  //namespace movelib {
}  //namespace boost {

#endif   //#define BOOST_MOVE_MERGE_HPP
