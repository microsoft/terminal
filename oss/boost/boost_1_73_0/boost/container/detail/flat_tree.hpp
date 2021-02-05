////////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_FLAT_TREE_HPP
#define BOOST_CONTAINER_FLAT_TREE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>

#include <boost/container/container_fwd.hpp>

#include <boost/move/utility_core.hpp>

#include <boost/container/detail/pair.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/allocator_traits.hpp>

#include <boost/container/detail/value_init.hpp>
#include <boost/container/detail/destroyers.hpp>
#include <boost/container/detail/algorithm.hpp> //algo_equal(), algo_lexicographical_compare
#include <boost/container/detail/iterator.hpp>
#include <boost/container/detail/is_sorted.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/detail/iterators.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/is_contiguous_container.hpp>
#include <boost/container/detail/is_container.hpp>

#include <boost/intrusive/detail/minimal_pair_header.hpp>      //pair

#include <boost/move/make_unique.hpp>
#include <boost/move/iterator.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/algo/adaptive_sort.hpp>
#include <boost/move/algo/detail/pdqsort.hpp>

#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//merge_unique
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME merge_unique
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 3
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//merge_equal
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME merge
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 3
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//index_of
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME index_of
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//nth
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME nth
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace dtl {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 0
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

namespace boost {
namespace container {
namespace dtl {

///////////////////////////////////////
//
// Helper functions to merge elements
//
///////////////////////////////////////

BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(stored_allocator_type)

///////////////////////////////////////
//
//  flat_tree_container_inplace_merge
//
///////////////////////////////////////
template<class SequenceContainer, class Compare>
void flat_tree_container_inplace_merge //is_contiguous_container == true
   (SequenceContainer& dest, typename SequenceContainer::iterator it, Compare comp , dtl::true_)
{
   typedef typename SequenceContainer::value_type  value_type;
   value_type *const braw = boost::movelib::iterator_to_raw_pointer(dest.begin());
   value_type *const iraw = boost::movelib::iterator_to_raw_pointer(it);
   value_type *const eraw = boost::movelib::iterator_to_raw_pointer(dest.end());
   boost::movelib::adaptive_merge(braw, iraw, eraw, comp, eraw, dest.capacity()- dest.size());
}

template<class SequenceContainer, class Compare>
void flat_tree_container_inplace_merge //is_contiguous_container == false
   (SequenceContainer& dest, typename SequenceContainer::iterator it, Compare comp, dtl::false_)
{
   boost::movelib::adaptive_merge(dest.begin(), it, dest.end(), comp);
}

///////////////////////////////////////
//
//  flat_tree_container_inplace_sort_ending
//
///////////////////////////////////////
template<class SequenceContainer, class Compare>
void flat_tree_container_inplace_sort_ending //is_contiguous_container == true
   (SequenceContainer& dest, typename SequenceContainer::iterator it, Compare comp, dtl::true_)
{
   typedef typename SequenceContainer::value_type  value_type;
   value_type *const iraw = boost::movelib::iterator_to_raw_pointer(it);
   value_type *const eraw = boost::movelib::iterator_to_raw_pointer(dest.end());
   boost::movelib::adaptive_sort(iraw, eraw, comp, eraw, dest.capacity()- dest.size());
}

template<class SequenceContainer, class Compare>
void flat_tree_container_inplace_sort_ending //is_contiguous_container == false
   (SequenceContainer& dest, typename SequenceContainer::iterator it, Compare comp , dtl::false_)
{
   boost::movelib::adaptive_sort(it, dest.end(), comp);
}

///////////////////////////////////////
//
//          flat_tree_merge
//
///////////////////////////////////////
template<class SequenceContainer, class Iterator, class Compare>
BOOST_CONTAINER_FORCEINLINE void flat_tree_merge_equal
   (SequenceContainer& dest, Iterator first, Iterator last, Compare comp, dtl::true_)
{
   dest.merge(first, last, comp);
}

template<class SequenceContainer, class Iterator, class Compare>
BOOST_CONTAINER_FORCEINLINE void flat_tree_merge_equal   //has_merge_unique == false
   (SequenceContainer& dest, Iterator first, Iterator last, Compare comp, dtl::false_)
{
   typedef typename SequenceContainer::iterator    iterator;
   iterator const it = dest.insert( dest.end(), first, last );
   dtl::bool_<is_contiguous_container<SequenceContainer>::value> contiguous_tag;
   (flat_tree_container_inplace_merge)(dest, it, comp, contiguous_tag);
}

///////////////////////////////////////
//
//       flat_tree_merge_unique
//
///////////////////////////////////////
template<class SequenceContainer, class Iterator, class Compare>
BOOST_CONTAINER_FORCEINLINE void flat_tree_merge_unique  //has_merge_unique == true
   (SequenceContainer& dest, Iterator first, Iterator last, Compare comp, dtl::true_)
{
   dest.merge_unique(first, last, comp);
}

template<class SequenceContainer, class Iterator, class Compare>
BOOST_CONTAINER_FORCEINLINE void flat_tree_merge_unique  //has_merge_unique == false
   (SequenceContainer& dest, Iterator first, Iterator last, Compare comp, dtl::false_)
{
   typedef typename SequenceContainer::iterator    iterator;
   typedef typename SequenceContainer::size_type   size_type;

   size_type const old_sz = dest.size();
   iterator const first_new = dest.insert(dest.cend(), first, last );
   iterator e = boost::movelib::inplace_set_unique_difference(first_new, dest.end(), dest.begin(), first_new, comp);
   dest.erase(e, dest.end());
   dtl::bool_<is_contiguous_container<SequenceContainer>::value> contiguous_tag;
   (flat_tree_container_inplace_merge)(dest, dest.begin()+old_sz, comp, contiguous_tag);
}

///////////////////////////////////////
//
//         flat_tree_index_of
//
///////////////////////////////////////
template<class SequenceContainer, class Iterator>
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::size_type
   flat_tree_index_of   // has_index_of == true
      (SequenceContainer& cont, Iterator p, dtl::true_)
{
   return cont.index_of(p);
}

template<class SequenceContainer, class Iterator>
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::size_type
   flat_tree_index_of   // has_index_of == false
      (SequenceContainer& cont, Iterator p, dtl::false_)
{
   typedef typename SequenceContainer::size_type size_type;
   return static_cast<size_type>(p - cont.begin());
}

///////////////////////////////////////
//
//         flat_tree_nth
//
///////////////////////////////////////
template<class Iterator, class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE Iterator
   flat_tree_nth  // has_nth == true
      (SequenceContainer& cont, typename SequenceContainer::size_type n, dtl::true_)
{
   return cont.nth(n);
}

template<class Iterator, class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE Iterator
   flat_tree_nth  // has_nth == false
      (SequenceContainer& cont, typename SequenceContainer::size_type n, dtl::false_)
{
   return cont.begin()+ n;
}

///////////////////////////////////////
//
//    flat_tree_get_stored_allocator
//
///////////////////////////////////////
template<class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::stored_allocator_type &
   flat_tree_get_stored_allocator   // has_get_stored_allocator == true
      (SequenceContainer& cont, dtl::true_)
{
   return cont.get_stored_allocator();
}

template<class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE const typename SequenceContainer::stored_allocator_type &
   flat_tree_get_stored_allocator   // has_get_stored_allocator == true
      (const SequenceContainer& cont, dtl::true_)
{
   return cont.get_stored_allocator();
}

template<class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::allocator_type
   flat_tree_get_stored_allocator   // has_get_stored_allocator == false
      (SequenceContainer& cont, dtl::false_)
{
   return cont.get_allocator();
}

///////////////////////////////////////
//
//    flat_tree_adopt_sequence_equal
//
///////////////////////////////////////
template<class SequenceContainer, class Compare>
void flat_tree_sort_contiguous_to_adopt // is_contiguous_container == true
   (SequenceContainer &tseq, BOOST_RV_REF(SequenceContainer) seq, Compare comp)
{
   if(tseq.capacity() >= (seq.capacity() - seq.size())) {
      tseq.clear();
      boost::movelib::adaptive_sort
      (boost::movelib::iterator_to_raw_pointer(seq.begin())
         , boost::movelib::iterator_to_raw_pointer(seq.end())
         , comp
         , boost::movelib::iterator_to_raw_pointer(tseq.begin())
         , tseq.capacity());
   }
   else{
      boost::movelib::adaptive_sort
      (boost::movelib::iterator_to_raw_pointer(seq.begin())
         , boost::movelib::iterator_to_raw_pointer(seq.end())
         , comp
         , boost::movelib::iterator_to_raw_pointer(seq.end())
         , seq.capacity() - seq.size());
   }
}

template<class SequenceContainer, class Compare>
void flat_tree_adopt_sequence_equal // is_contiguous_container == true
   (SequenceContainer &tseq, BOOST_RV_REF(SequenceContainer) seq, Compare comp, dtl::true_)
{
   flat_tree_sort_contiguous_to_adopt(tseq, boost::move(seq), comp);
   tseq = boost::move(seq);
}

template<class SequenceContainer, class Compare>
void flat_tree_adopt_sequence_equal // is_contiguous_container == false
   (SequenceContainer &tseq, BOOST_RV_REF(SequenceContainer) seq, Compare comp, dtl::false_)
{
   boost::movelib::adaptive_sort(seq.begin(), seq.end(), comp);
   tseq = boost::move(seq);
}

///////////////////////////////////////
//
//    flat_tree_adopt_sequence_unique
//
///////////////////////////////////////
template<class SequenceContainer, class Compare>
void flat_tree_adopt_sequence_unique// is_contiguous_container == true
   (SequenceContainer &tseq, BOOST_RV_REF(SequenceContainer) seq, Compare comp, dtl::true_)
{
   boost::movelib::pdqsort
      ( boost::movelib::iterator_to_raw_pointer(seq.begin())
      , boost::movelib::iterator_to_raw_pointer(seq.end())
      , comp);
   seq.erase(boost::movelib::unique
      (seq.begin(), seq.end(), boost::movelib::negate<Compare>(comp)), seq.cend());
   tseq = boost::move(seq);
}

template<class SequenceContainer, class Compare>
void flat_tree_adopt_sequence_unique// is_contiguous_container == false
   (SequenceContainer &tseq, BOOST_RV_REF(SequenceContainer) seq, Compare comp, dtl::false_)
{
   boost::movelib::pdqsort(seq.begin(), seq.end(), comp);
   seq.erase(boost::movelib::unique
      (seq.begin(), seq.end(), boost::movelib::negate<Compare>(comp)), seq.cend());
   tseq = boost::move(seq);
}

///////////////////////////////////////
//
//       flat_tree_reserve
//
///////////////////////////////////////
template<class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE void // has_reserve == true
   flat_tree_reserve(SequenceContainer &tseq, typename SequenceContainer::size_type cap, dtl::true_)
{
   tseq.reserve(cap);
}

template<class SequenceContainer>
BOOST_CONTAINER_FORCEINLINE void // has_reserve == false
   flat_tree_reserve(SequenceContainer &, typename SequenceContainer::size_type, dtl::false_)
{
}

///////////////////////////////////////
//
//       flat_tree_capacity
//
///////////////////////////////////////
template<class SequenceContainer>   // has_capacity == true
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::size_type
   flat_tree_capacity(const SequenceContainer &tseq, dtl::true_)
{
   return tseq.capacity();
}

template<class SequenceContainer>   // has_capacity == false
BOOST_CONTAINER_FORCEINLINE typename SequenceContainer::size_type
   flat_tree_capacity(const SequenceContainer &tseq, dtl::false_)
{
   return tseq.size();
}

///////////////////////////////////////
//
//       flat_tree_value_compare
//
///////////////////////////////////////

template<class Compare, class Value, class KeyOfValue>
class flat_tree_value_compare
   : private Compare
{
   typedef Value              first_argument_type;
   typedef Value              second_argument_type;
   typedef bool               return_type;
   public:
   flat_tree_value_compare()
      : Compare()
   {}

   flat_tree_value_compare(const Compare &pred)
      : Compare(pred)
   {}

   bool operator()(const Value& lhs, const Value& rhs) const
   {
      KeyOfValue key_extract;
      return Compare::operator()(key_extract(lhs), key_extract(rhs));
   }

   const Compare &get_comp() const
      {  return *this;  }

   Compare &get_comp()
      {  return *this;  }
};


///////////////////////////////////////
//
//       select_container_type
//
///////////////////////////////////////
template < class Value, class AllocatorOrContainer
         , bool = boost::container::dtl::is_container<AllocatorOrContainer>::value
         >
struct select_container_type
{
   typedef AllocatorOrContainer type;
};

template <class Value, class AllocatorOrContainer>
struct select_container_type<Value, AllocatorOrContainer, false>
{
   typedef boost::container::vector<Value, typename real_allocator<Value, AllocatorOrContainer>::type> type;
};


///////////////////////////////////////
//
//          flat_tree
//
///////////////////////////////////////
template <class Value, class KeyOfValue,
          class Compare, class AllocatorOrContainer>
class flat_tree
{
   public:
   typedef typename select_container_type<Value, AllocatorOrContainer>::type container_type;
   typedef container_type sequence_type;  //For backwards compatibility

   private:
   typedef typename container_type::allocator_type        allocator_t;
   typedef allocator_traits<allocator_t>                 allocator_traits_type;

   public:
   typedef flat_tree_value_compare<Compare, Value, KeyOfValue> value_compare;

   private:
   
   struct Data
      //Inherit from value_compare to do EBO
      : public value_compare
   {
      BOOST_COPYABLE_AND_MOVABLE(Data)

      public:
      Data()
         : value_compare(), m_seq()
      {}

      explicit Data(const allocator_t &alloc)
         : value_compare(), m_seq(alloc)
      {}

      explicit Data(const Compare &comp)
         : value_compare(comp), m_seq()
      {}

      Data(const Compare &comp, const allocator_t &alloc)
         : value_compare(comp), m_seq(alloc)
      {}

      explicit Data(const Data &d)
         : value_compare(static_cast<const value_compare&>(d)), m_seq(d.m_seq)
      {}

      Data(BOOST_RV_REF(Data) d)
         : value_compare(boost::move(static_cast<value_compare&>(d))), m_seq(boost::move(d.m_seq))
      {}

      Data(const Data &d, const allocator_t &a)
         : value_compare(static_cast<const value_compare&>(d)), m_seq(d.m_seq, a)
      {}

      Data(BOOST_RV_REF(Data) d, const allocator_t &a)
         : value_compare(boost::move(static_cast<value_compare&>(d))), m_seq(boost::move(d.m_seq), a)
      {}

      Data& operator=(BOOST_COPY_ASSIGN_REF(Data) d)
      {
         this->value_compare::operator=(d);
         m_seq = d.m_seq;
         return *this;
      }

      Data& operator=(BOOST_RV_REF(Data) d)
      {
         this->value_compare::operator=(boost::move(static_cast<value_compare &>(d)));
         m_seq = boost::move(d.m_seq);
         return *this;
      }

      void swap(Data &d)
      {
         value_compare& mycomp    = *this, & othercomp = d;
         boost::adl_move_swap(mycomp, othercomp);
         this->m_seq.swap(d.m_seq);
      }

      container_type m_seq;
   };

   Data m_data;
   BOOST_COPYABLE_AND_MOVABLE(flat_tree)

   public:

   typedef typename container_type::value_type               value_type;
   typedef typename container_type::pointer                  pointer;
   typedef typename container_type::const_pointer            const_pointer;
   typedef typename container_type::reference                reference;
   typedef typename container_type::const_reference          const_reference;
   typedef typename KeyOfValue::type                        key_type;
   typedef Compare                                          key_compare;
   typedef typename container_type::allocator_type           allocator_type;
   typedef typename container_type::size_type                size_type;
   typedef typename container_type::difference_type          difference_type;
   typedef typename container_type::iterator                 iterator;
   typedef typename container_type::const_iterator           const_iterator;
   typedef typename container_type::reverse_iterator         reverse_iterator;
   typedef typename container_type::const_reverse_iterator   const_reverse_iterator;

   //!Standard extension
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      (boost::container::dtl::, container_type
      ,stored_allocator_type, allocator_type)               stored_allocator_type;

   static const bool has_stored_allocator_type =
      BOOST_INTRUSIVE_HAS_TYPE(boost::container::dtl::, container_type, stored_allocator_type);

   private:
   typedef allocator_traits<stored_allocator_type> stored_allocator_traits;

   public:
   typedef typename dtl::if_c
      <has_stored_allocator_type, const stored_allocator_type &, allocator_type>::type get_stored_allocator_const_return_t;

   typedef typename dtl::if_c
      <has_stored_allocator_type, stored_allocator_type &, allocator_type>::type get_stored_allocator_noconst_return_t;

   BOOST_CONTAINER_FORCEINLINE flat_tree()
      : m_data()
   { }

   BOOST_CONTAINER_FORCEINLINE explicit flat_tree(const Compare& comp)
      : m_data(comp)
   { }

   BOOST_CONTAINER_FORCEINLINE explicit flat_tree(const allocator_type& a)
      : m_data(a)
   { }

   BOOST_CONTAINER_FORCEINLINE flat_tree(const Compare& comp, const allocator_type& a)
      : m_data(comp, a)
   { }

   BOOST_CONTAINER_FORCEINLINE flat_tree(const flat_tree& x)
      :  m_data(x.m_data)
   { }

   BOOST_CONTAINER_FORCEINLINE flat_tree(BOOST_RV_REF(flat_tree) x)
      BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_constructible<Compare>::value)
      :  m_data(boost::move(x.m_data))
   { }

   BOOST_CONTAINER_FORCEINLINE flat_tree(const flat_tree& x, const allocator_type &a)
      :  m_data(x.m_data, a)
   { }

   BOOST_CONTAINER_FORCEINLINE flat_tree(BOOST_RV_REF(flat_tree) x, const allocator_type &a)
      :  m_data(boost::move(x.m_data), a)
   { }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_range_t, InputIterator first, InputIterator last)
      : m_data()
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_range_t, InputIterator first, InputIterator last, const Compare& comp)
      : m_data(comp)
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_range_t, InputIterator first, InputIterator last, const Compare& comp, const allocator_type& a)
      : m_data(comp, a)
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_unique_range_t, InputIterator first, InputIterator last)
      : m_data()
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted_and_unique)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_unique_range_t, InputIterator first, InputIterator last, const Compare& comp)
      : m_data(comp)
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted_and_unique)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( ordered_unique_range_t, InputIterator first, InputIterator last, const Compare& comp, const allocator_type& a)
      : m_data(comp, a)
   {
      this->m_data.m_seq.insert(this->m_data.m_seq.end(), first, last);
      BOOST_ASSERT((is_sorted_and_unique)(this->m_data.m_seq.cbegin(), this->m_data.m_seq.cend(), this->priv_value_comp()));
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( bool unique_insertion, InputIterator first, InputIterator last)
      : m_data()
   {
      this->priv_range_insertion_construct(unique_insertion, first, last);
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( bool unique_insertion, InputIterator first, InputIterator last
            , const Compare& comp)
      : m_data(comp)
   {
      this->priv_range_insertion_construct(unique_insertion, first, last);
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( bool unique_insertion, InputIterator first, InputIterator last
            , const allocator_type& a)
      : m_data(a)
   {
      this->priv_range_insertion_construct(unique_insertion, first, last);
   }

   template <class InputIterator>
   BOOST_CONTAINER_FORCEINLINE
   flat_tree( bool unique_insertion, InputIterator first, InputIterator last
            , const Compare& comp, const allocator_type& a)
      : m_data(comp, a)
   {
      this->priv_range_insertion_construct(unique_insertion, first, last);
   }

   BOOST_CONTAINER_FORCEINLINE ~flat_tree()
   {}

   BOOST_CONTAINER_FORCEINLINE flat_tree&  operator=(BOOST_COPY_ASSIGN_REF(flat_tree) x)
   {  m_data = x.m_data;   return *this;  }

   BOOST_CONTAINER_FORCEINLINE flat_tree&  operator=(BOOST_RV_REF(flat_tree) x)
      BOOST_NOEXCEPT_IF( (allocator_traits_type::propagate_on_container_move_assignment::value ||
                          allocator_traits_type::is_always_equal::value) &&
                           boost::container::dtl::is_nothrow_move_assignable<Compare>::value)
   {  m_data = boost::move(x.m_data); return *this;  }

   BOOST_CONTAINER_FORCEINLINE const value_compare &priv_value_comp() const
   { return static_cast<const value_compare &>(this->m_data); }

   BOOST_CONTAINER_FORCEINLINE value_compare &priv_value_comp()
   { return static_cast<value_compare &>(this->m_data); }

   BOOST_CONTAINER_FORCEINLINE const key_compare &priv_key_comp() const
   { return this->priv_value_comp().get_comp(); }

   BOOST_CONTAINER_FORCEINLINE key_compare &priv_key_comp()
   { return this->priv_value_comp().get_comp(); }

   struct insert_commit_data
   {
      const_iterator position;
   };

   public:
   // accessors:
   BOOST_CONTAINER_FORCEINLINE Compare key_comp() const
   { return this->m_data.get_comp(); }

   BOOST_CONTAINER_FORCEINLINE value_compare value_comp() const
   { return this->m_data; }

   BOOST_CONTAINER_FORCEINLINE allocator_type get_allocator() const
   { return this->m_data.m_seq.get_allocator(); }

   BOOST_CONTAINER_FORCEINLINE get_stored_allocator_const_return_t get_stored_allocator() const
   {
      return flat_tree_get_stored_allocator(this->m_data.m_seq, dtl::bool_<has_stored_allocator_type>());
   }

   BOOST_CONTAINER_FORCEINLINE get_stored_allocator_noconst_return_t get_stored_allocator()
   {
      return flat_tree_get_stored_allocator(this->m_data.m_seq, dtl::bool_<has_stored_allocator_type>());
   }

   BOOST_CONTAINER_FORCEINLINE iterator begin()
   { return this->m_data.m_seq.begin(); }

   BOOST_CONTAINER_FORCEINLINE const_iterator begin() const
   { return this->cbegin(); }

   BOOST_CONTAINER_FORCEINLINE const_iterator cbegin() const
   { return this->m_data.m_seq.begin(); }

   BOOST_CONTAINER_FORCEINLINE iterator end()
   { return this->m_data.m_seq.end(); }

   BOOST_CONTAINER_FORCEINLINE const_iterator end() const
   { return this->cend(); }

   BOOST_CONTAINER_FORCEINLINE const_iterator cend() const
   { return this->m_data.m_seq.end(); }

   BOOST_CONTAINER_FORCEINLINE reverse_iterator rbegin()
   { return reverse_iterator(this->end()); }

   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator rbegin() const
   {  return this->crbegin();  }

   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator crbegin() const
   {  return const_reverse_iterator(this->cend());  }

   BOOST_CONTAINER_FORCEINLINE reverse_iterator rend()
   { return reverse_iterator(this->begin()); }

   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator rend() const
   { return this->crend(); }

   BOOST_CONTAINER_FORCEINLINE const_reverse_iterator crend() const
   { return const_reverse_iterator(this->cbegin()); }

   BOOST_CONTAINER_FORCEINLINE bool empty() const
   { return this->m_data.m_seq.empty(); }

   BOOST_CONTAINER_FORCEINLINE size_type size() const
   { return this->m_data.m_seq.size(); }

   BOOST_CONTAINER_FORCEINLINE size_type max_size() const
   { return this->m_data.m_seq.max_size(); }

   BOOST_CONTAINER_FORCEINLINE void swap(flat_tree& other)
      BOOST_NOEXCEPT_IF(  allocator_traits_type::is_always_equal::value
                                 && boost::container::dtl::is_nothrow_swappable<Compare>::value )
   {  this->m_data.swap(other.m_data);  }

   public:
   // insert/erase
   std::pair<iterator,bool> insert_unique(const value_type& val)
   {
      std::pair<iterator,bool> ret;
      insert_commit_data data;
      ret.second = this->priv_insert_unique_prepare(KeyOfValue()(val), data);
      ret.first = ret.second ? this->priv_insert_commit(data, val)
                             : this->begin() + (data.position - this->cbegin());
                             //: iterator(vector_iterator_get_ptr(data.position));
      return ret;
   }

   std::pair<iterator,bool> insert_unique(BOOST_RV_REF(value_type) val)
   {
      std::pair<iterator,bool> ret;
      insert_commit_data data;
      ret.second = this->priv_insert_unique_prepare(KeyOfValue()(val), data);
      ret.first = ret.second ? this->priv_insert_commit(data, boost::move(val))
                             : this->begin() + (data.position - this->cbegin());
                             //: iterator(vector_iterator_get_ptr(data.position));
      return ret;
   }

   iterator insert_equal(const value_type& val)
   {
      iterator i = this->upper_bound(KeyOfValue()(val));
      i = this->m_data.m_seq.insert(i, val);
      return i;
   }

   iterator insert_equal(BOOST_RV_REF(value_type) mval)
   {
      iterator i = this->upper_bound(KeyOfValue()(mval));
      i = this->m_data.m_seq.insert(i, boost::move(mval));
      return i;
   }

   iterator insert_unique(const_iterator hint, const value_type& val)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(hint));
      insert_commit_data data;
      return this->priv_insert_unique_prepare(hint, KeyOfValue()(val), data)
            ? this->priv_insert_commit(data, val)
            : this->begin() + (data.position - this->cbegin());
            //: iterator(vector_iterator_get_ptr(data.position));
   }

   iterator insert_unique(const_iterator hint, BOOST_RV_REF(value_type) val)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(hint));
      insert_commit_data data;
      return this->priv_insert_unique_prepare(hint, KeyOfValue()(val), data)
         ? this->priv_insert_commit(data, boost::move(val))
         : this->begin() + (data.position - this->cbegin());
         //: iterator(vector_iterator_get_ptr(data.position));
   }

   iterator insert_equal(const_iterator hint, const value_type& val)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(hint));
      insert_commit_data data;
      this->priv_insert_equal_prepare(hint, val, data);
      return this->priv_insert_commit(data, val);
   }

   iterator insert_equal(const_iterator hint, BOOST_RV_REF(value_type) mval)
   {
      BOOST_ASSERT(this->priv_in_range_or_end(hint));
      insert_commit_data data;
      this->priv_insert_equal_prepare(hint, mval, data);
      return this->priv_insert_commit(data, boost::move(mval));
   }

   template <class InIt>
   void insert_unique(InIt first, InIt last)
   {
      dtl::bool_<is_contiguous_container<container_type>::value> contiguous_tag;
      container_type &seq = this->m_data.m_seq;
      value_compare &val_cmp = this->priv_value_comp();

      //Step 1: put new elements in the back
      typename container_type::iterator const it = seq.insert(seq.cend(), first, last);

      //Step 2: sort them
      boost::movelib::pdqsort(it, seq.end(), val_cmp);

      //Step 3: only left unique values from the back not already present in the original range
      typename container_type::iterator const e = boost::movelib::inplace_set_unique_difference
         (it, seq.end(), seq.begin(), it, val_cmp);

      seq.erase(e, seq.cend());
      //it might be invalidated by erasing [e, seq.end) if e == it
      if (it != e)
      {
         //Step 4: merge both ranges
         (flat_tree_container_inplace_merge)(seq, it, this->priv_value_comp(), contiguous_tag);
      }
   }

   template <class InIt>
   void insert_equal(InIt first, InIt last)
   {
      dtl::bool_<is_contiguous_container<container_type>::value> contiguous_tag;
      container_type &seq = this->m_data.m_seq;
      typename container_type::iterator const it = seq.insert(seq.cend(), first, last);
      (flat_tree_container_inplace_sort_ending)(seq, it, this->priv_value_comp(), contiguous_tag);
      (flat_tree_container_inplace_merge)      (seq, it, this->priv_value_comp(), contiguous_tag);
   }

   //Ordered

   template <class InIt>
   void insert_equal(ordered_range_t, InIt first, InIt last)
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_merge_unique<container_type, InIt, InIt, value_compare>::value;
      (flat_tree_merge_equal)(this->m_data.m_seq, first, last, this->priv_value_comp(), dtl::bool_<value>());
   }

   template <class InIt>
   void insert_unique(ordered_unique_range_t, InIt first, InIt last)
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_merge_unique<container_type, InIt, InIt, value_compare>::value;
      (flat_tree_merge_unique)(this->m_data.m_seq, first, last, this->priv_value_comp(), dtl::bool_<value>());
   }

   #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template <class... Args>
   std::pair<iterator, bool> emplace_unique(BOOST_FWD_REF(Args)... args)
   {
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
      value_type *pval = reinterpret_cast<value_type *>(v.data);
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();
      stored_allocator_traits::construct(a, pval, ::boost::forward<Args>(args)... );
      value_destructor<stored_allocator_type, value_type> d(a, *pval);
      return this->insert_unique(::boost::move(*pval));
   }

   template <class... Args>
   iterator emplace_hint_unique(const_iterator hint, BOOST_FWD_REF(Args)... args)
   {
      //hint checked in insert_unique
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
      value_type *pval = reinterpret_cast<value_type *>(v.data);
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();
      stored_allocator_traits::construct(a, pval, ::boost::forward<Args>(args)... );
      value_destructor<stored_allocator_type, value_type> d(a, *pval);
      return this->insert_unique(hint, ::boost::move(*pval));
   }

   template <class... Args>
   iterator emplace_equal(BOOST_FWD_REF(Args)... args)
   {
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
      value_type *pval = reinterpret_cast<value_type *>(v.data);
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();
      stored_allocator_traits::construct(a, pval, ::boost::forward<Args>(args)... );
      value_destructor<stored_allocator_type, value_type> d(a, *pval);
      return this->insert_equal(::boost::move(*pval));
   }

   template <class... Args>
   iterator emplace_hint_equal(const_iterator hint, BOOST_FWD_REF(Args)... args)
   {
      //hint checked in insert_equal
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;
      value_type *pval = reinterpret_cast<value_type *>(v.data);
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();
      stored_allocator_traits::construct(a, pval, ::boost::forward<Args>(args)... );
      value_destructor<stored_allocator_type, value_type> d(a, *pval);
      return this->insert_equal(hint, ::boost::move(*pval));
   }

   template <class KeyType, class... Args>
   BOOST_CONTAINER_FORCEINLINE std::pair<iterator, bool> try_emplace
      (const_iterator hint, BOOST_FWD_REF(KeyType) key, BOOST_FWD_REF(Args)... args)
   {
      std::pair<iterator,bool> ret;
      insert_commit_data data;
      const key_type & k = key;
      ret.second = hint == const_iterator()
         ? this->priv_insert_unique_prepare(k, data)
         : this->priv_insert_unique_prepare(hint, k, data);

      if(!ret.second){
         ret.first  = this->nth(data.position - this->cbegin());
      }
      else{
         typedef typename emplace_functor_type<try_emplace_t, KeyType, Args...>::type func_t;
         typedef emplace_iterator<value_type, func_t, difference_type> it_t;
         func_t func(try_emplace_t(), ::boost::forward<KeyType>(key), ::boost::forward<Args>(args)...);
         ret.first = this->m_data.m_seq.insert(data.position, it_t(func), it_t());
      }
      return ret;
   }

   #else // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   #define BOOST_CONTAINER_FLAT_TREE_EMPLACE_CODE(N) \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   std::pair<iterator, bool> emplace_unique(BOOST_MOVE_UREF##N)\
   {\
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;\
      value_type *pval = reinterpret_cast<value_type *>(v.data);\
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();\
      stored_allocator_traits::construct(a, pval BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      value_destructor<stored_allocator_type, value_type> d(a, *pval);\
      return this->insert_unique(::boost::move(*pval));\
   }\
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   iterator emplace_hint_unique(const_iterator hint BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;\
      value_type *pval = reinterpret_cast<value_type *>(v.data);\
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();\
      stored_allocator_traits::construct(a, pval BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      value_destructor<stored_allocator_type, value_type> d(a, *pval);\
      return this->insert_unique(hint, ::boost::move(*pval));\
   }\
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   iterator emplace_equal(BOOST_MOVE_UREF##N)\
   {\
      typename dtl::aligned_storage<sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;\
      value_type *pval = reinterpret_cast<value_type *>(v.data);\
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();\
      stored_allocator_traits::construct(a, pval BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      value_destructor<stored_allocator_type, value_type> d(a, *pval);\
      return this->insert_equal(::boost::move(*pval));\
   }\
   \
   BOOST_MOVE_TMPL_LT##N BOOST_MOVE_CLASS##N BOOST_MOVE_GT##N \
   iterator emplace_hint_equal(const_iterator hint BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      typename dtl::aligned_storage <sizeof(value_type), dtl::alignment_of<value_type>::value>::type v;\
      value_type *pval = reinterpret_cast<value_type *>(v.data);\
      get_stored_allocator_noconst_return_t a = this->get_stored_allocator();\
      stored_allocator_traits::construct(a, pval BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      value_destructor<stored_allocator_type, value_type> d(a, *pval);\
      return this->insert_equal(hint, ::boost::move(*pval));\
   }\
   template <class KeyType BOOST_MOVE_I##N BOOST_MOVE_CLASS##N>\
   BOOST_CONTAINER_FORCEINLINE std::pair<iterator, bool>\
      try_emplace(const_iterator hint, BOOST_FWD_REF(KeyType) key BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      std::pair<iterator,bool> ret;\
      insert_commit_data data;\
      const key_type & k = key;\
      ret.second = hint == const_iterator()\
         ? this->priv_insert_unique_prepare(k, data)\
         : this->priv_insert_unique_prepare(hint, k, data);\
      \
      if(!ret.second){\
         ret.first  = this->nth(data.position - this->cbegin());\
      }\
      else{\
         typedef typename emplace_functor_type<try_emplace_t, KeyType BOOST_MOVE_I##N BOOST_MOVE_TARG##N>::type func_t;\
         typedef emplace_iterator<value_type, func_t, difference_type> it_t;\
         func_t func(try_emplace_t(), ::boost::forward<KeyType>(key) BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
         ret.first = this->m_data.m_seq.insert(data.position, it_t(func), it_t());\
      }\
      return ret;\
   }\
   //
   BOOST_MOVE_ITERATE_0TO7(BOOST_CONTAINER_FLAT_TREE_EMPLACE_CODE)
   #undef BOOST_CONTAINER_FLAT_TREE_EMPLACE_CODE

   #endif   // !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

   template<class KeyType, class M>
   std::pair<iterator, bool> insert_or_assign(const_iterator hint, BOOST_FWD_REF(KeyType) key, BOOST_FWD_REF(M) obj)
   {
      const key_type& k = key;
      std::pair<iterator,bool> ret;
      insert_commit_data data;
      ret.second = hint == const_iterator()
         ? this->priv_insert_unique_prepare(k, data)
         : this->priv_insert_unique_prepare(hint, k, data);
      if(!ret.second){
         ret.first  = this->nth(data.position - this->cbegin());
         ret.first->second = boost::forward<M>(obj);
      }
      else{
         typedef typename emplace_functor_type<KeyType, M>::type func_t;
         typedef emplace_iterator<value_type, func_t, difference_type> it_t;
         func_t func(boost::forward<KeyType>(key), boost::forward<M>(obj));
         ret.first = this->m_data.m_seq.insert(data.position, it_t(func), it_t());
      }
      return ret;
   }

   BOOST_CONTAINER_FORCEINLINE iterator erase(const_iterator position)
   {  return this->m_data.m_seq.erase(position);  }

   size_type erase(const key_type& k)
   {
      std::pair<iterator,iterator > itp = this->equal_range(k);
      size_type ret = static_cast<size_type>(itp.second-itp.first);
      if (ret){
         this->m_data.m_seq.erase(itp.first, itp.second);
      }
      return ret;
   }

   BOOST_CONTAINER_FORCEINLINE iterator erase(const_iterator first, const_iterator last)
   {  return this->m_data.m_seq.erase(first, last);  }

   BOOST_CONTAINER_FORCEINLINE void clear()
   {  this->m_data.m_seq.clear();  }

   //! <b>Effects</b>: Tries to deallocate the excess of memory created
   //    with previous allocations. The size of the vector is unchanged
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to size().
   BOOST_CONTAINER_FORCEINLINE void shrink_to_fit()
   {  this->m_data.m_seq.shrink_to_fit();  }

   BOOST_CONTAINER_FORCEINLINE iterator nth(size_type n) BOOST_NOEXCEPT_OR_NOTHROW
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_nth<container_type, size_type>::value;
      return flat_tree_nth<iterator>(this->m_data.m_seq, n, dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE const_iterator nth(size_type n) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_nth<container_type, size_type>::value;
      return flat_tree_nth<const_iterator>(this->m_data.m_seq, n, dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE size_type index_of(iterator p) BOOST_NOEXCEPT_OR_NOTHROW
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_index_of<container_type, iterator>::value;
      return flat_tree_index_of(this->m_data.m_seq, p, dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE size_type index_of(const_iterator p) const BOOST_NOEXCEPT_OR_NOTHROW
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_index_of<container_type, const_iterator>::value;
      return flat_tree_index_of(this->m_data.m_seq, p, dtl::bool_<value>());
   }

   // set operations:
   iterator find(const key_type& k)
   {
      iterator i = this->lower_bound(k);
      iterator end_it = this->end();
      if (i != end_it && this->m_data.get_comp()(k, KeyOfValue()(*i))){
         i = end_it;
      }
      return i;
   }

   const_iterator find(const key_type& k) const
   {
      const_iterator i = this->lower_bound(k);

      const_iterator end_it = this->cend();
      if (i != end_it && this->m_data.get_comp()(k, KeyOfValue()(*i))){
         i = end_it;
      }
      return i;
   }

   template<class K>
   typename dtl::enable_if_transparent<key_compare, K, iterator>::type
      find(const K& k)
   {
      iterator i = this->lower_bound(k);
      iterator end_it = this->end();
      if (i != end_it && this->m_data.get_comp()(k, KeyOfValue()(*i))){
         i = end_it;
      }
      return i;
   }

   template<class K>
   typename dtl::enable_if_transparent<key_compare, K, const_iterator>::type
      find(const K& k) const
   {
      const_iterator i = this->lower_bound(k);

      const_iterator end_it = this->cend();
      if (i != end_it && this->m_data.get_comp()(k, KeyOfValue()(*i))){
         i = end_it;
      }
      return i;
   }

   size_type count(const key_type& k) const
   {
      std::pair<const_iterator, const_iterator> p = this->equal_range(k);
      size_type n = p.second - p.first;
      return n;
   }

   template<class K>
   typename dtl::enable_if_transparent<key_compare, K, size_type>::type
      count(const K& k) const
   {
      std::pair<const_iterator, const_iterator> p = this->equal_range(k);
      size_type n = p.second - p.first;
      return n;
   }

   BOOST_CONTAINER_FORCEINLINE bool contains(const key_type& x) const
   {  return this->find(x) != this->cend();  }

   template<typename K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K, bool>::type
         contains(const K& x) const
   {  return this->find(x) != this->cend();  }

   template<class C2>
   BOOST_CONTAINER_FORCEINLINE void merge_unique(flat_tree<Value, KeyOfValue, C2, AllocatorOrContainer>& source)
   {
      this->insert_unique( boost::make_move_iterator(source.begin())
                         , boost::make_move_iterator(source.end()));
   }

   template<class C2>
   BOOST_CONTAINER_FORCEINLINE void merge_equal(flat_tree<Value, KeyOfValue, C2, AllocatorOrContainer>& source)
   {
      this->insert_equal( boost::make_move_iterator(source.begin())
                        , boost::make_move_iterator(source.end()));
   }

   BOOST_CONTAINER_FORCEINLINE void merge_unique(flat_tree& source)
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_merge_unique<container_type, iterator, iterator, value_compare>::value;
      (flat_tree_merge_unique)
         ( this->m_data.m_seq
         , boost::make_move_iterator(source.m_data.m_seq.begin())
         , boost::make_move_iterator(source.m_data.m_seq.end())
         , this->priv_value_comp()
         , dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE void merge_equal(flat_tree& source)
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_merge<container_type, iterator, iterator, value_compare>::value;
      (flat_tree_merge_equal)
         ( this->m_data.m_seq
         , boost::make_move_iterator(source.m_data.m_seq.begin())
         , boost::make_move_iterator(source.m_data.m_seq.end())
         , this->priv_value_comp()
         , dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE iterator lower_bound(const key_type& k)
   {  return this->priv_lower_bound(this->begin(), this->end(), k);  }

   BOOST_CONTAINER_FORCEINLINE const_iterator lower_bound(const key_type& k) const
   {  return this->priv_lower_bound(this->cbegin(), this->cend(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE 
      typename dtl::enable_if_transparent<key_compare, K, iterator>::type
         lower_bound(const K& k)
   {  return this->priv_lower_bound(this->begin(), this->end(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE 
      typename dtl::enable_if_transparent<key_compare, K, const_iterator>::type
         lower_bound(const K& k) const
   {  return this->priv_lower_bound(this->cbegin(), this->cend(), k);  }

   BOOST_CONTAINER_FORCEINLINE iterator upper_bound(const key_type& k)
   {  return this->priv_upper_bound(this->begin(), this->end(), k);  }

   BOOST_CONTAINER_FORCEINLINE const_iterator upper_bound(const key_type& k) const
   {  return this->priv_upper_bound(this->cbegin(), this->cend(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,iterator>::type
   upper_bound(const K& k)
   {  return this->priv_upper_bound(this->begin(), this->end(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,const_iterator>::type
         upper_bound(const K& k) const
   {  return this->priv_upper_bound(this->cbegin(), this->cend(), k);  }

   BOOST_CONTAINER_FORCEINLINE std::pair<iterator,iterator> equal_range(const key_type& k)
   {  return this->priv_equal_range(this->begin(), this->end(), k);  }

   BOOST_CONTAINER_FORCEINLINE std::pair<const_iterator, const_iterator> equal_range(const key_type& k) const
   {  return this->priv_equal_range(this->cbegin(), this->cend(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,std::pair<iterator,iterator> >::type
         equal_range(const K& k)
   {  return this->priv_equal_range(this->begin(), this->end(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,std::pair<const_iterator,const_iterator> >::type
         equal_range(const K& k) const
   {  return this->priv_equal_range(this->cbegin(), this->cend(), k);  }


   BOOST_CONTAINER_FORCEINLINE std::pair<iterator, iterator> lower_bound_range(const key_type& k)
   {  return this->priv_lower_bound_range(this->begin(), this->end(), k);  }

   BOOST_CONTAINER_FORCEINLINE std::pair<const_iterator, const_iterator> lower_bound_range(const key_type& k) const
   {  return this->priv_lower_bound_range(this->cbegin(), this->cend(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,std::pair<iterator,iterator> >::type
         lower_bound_range(const K& k)
   {  return this->priv_lower_bound_range(this->begin(), this->end(), k);  }

   template<class K>
   BOOST_CONTAINER_FORCEINLINE
      typename dtl::enable_if_transparent<key_compare, K,std::pair<const_iterator,const_iterator> >::type
         lower_bound_range(const K& k) const
   {  return this->priv_lower_bound_range(this->cbegin(), this->cend(), k);  }

   BOOST_CONTAINER_FORCEINLINE size_type capacity() const
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_capacity<container_type>::value;
      return (flat_tree_capacity)(this->m_data.m_seq, dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE void reserve(size_type cnt)
   {
      const bool value = boost::container::dtl::
         has_member_function_callable_with_reserve<container_type, size_type>::value;
      (flat_tree_reserve)(this->m_data.m_seq, cnt, dtl::bool_<value>());
   }

   BOOST_CONTAINER_FORCEINLINE container_type extract_sequence()
   {
      return boost::move(m_data.m_seq);
   }

   BOOST_CONTAINER_FORCEINLINE container_type &get_sequence_ref()
   {
      return m_data.m_seq;
   }

   BOOST_CONTAINER_FORCEINLINE void adopt_sequence_equal(BOOST_RV_REF(container_type) seq)
   {
      (flat_tree_adopt_sequence_equal)( m_data.m_seq, boost::move(seq), this->priv_value_comp()
         , dtl::bool_<is_contiguous_container<container_type>::value>());
   }

   BOOST_CONTAINER_FORCEINLINE void adopt_sequence_unique(BOOST_RV_REF(container_type) seq)
   {
      (flat_tree_adopt_sequence_unique)(m_data.m_seq, boost::move(seq), this->priv_value_comp()
         , dtl::bool_<is_contiguous_container<container_type>::value>());
   }

   void adopt_sequence_equal(ordered_range_t, BOOST_RV_REF(container_type) seq)
   {
      BOOST_ASSERT((is_sorted)(seq.cbegin(), seq.cend(), this->priv_value_comp()));
      m_data.m_seq = boost::move(seq);
   }

   void adopt_sequence_unique(ordered_unique_range_t, BOOST_RV_REF(container_type) seq)
   {
      BOOST_ASSERT((is_sorted_and_unique)(seq.cbegin(), seq.cend(), this->priv_value_comp()));
      m_data.m_seq = boost::move(seq);
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const flat_tree& x, const flat_tree& y)
   {
      return x.size() == y.size() && ::boost::container::algo_equal(x.begin(), x.end(), y.begin());
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<(const flat_tree& x, const flat_tree& y)
   {
      return ::boost::container::algo_lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
   }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const flat_tree& x, const flat_tree& y)
      {  return !(x == y); }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>(const flat_tree& x, const flat_tree& y)
      {  return y < x;  }

   BOOST_CONTAINER_FORCEINLINE friend bool operator<=(const flat_tree& x, const flat_tree& y)
      {  return !(y < x);  }

   BOOST_CONTAINER_FORCEINLINE friend bool operator>=(const flat_tree& x, const flat_tree& y)
      {  return !(x < y);  }

   BOOST_CONTAINER_FORCEINLINE friend void swap(flat_tree& x, flat_tree& y)
      {  x.swap(y);  }

   private:

   template <class InputIterator>
   void priv_range_insertion_construct( bool unique_insertion, InputIterator first, InputIterator last)
   {
      //Use cend() as hint to achieve linear time for
      //ordered ranges as required by the standard
      //for the constructor
      //Call end() every iteration as reallocation might have invalidated iterators
      if(unique_insertion){
         this->insert_unique(first, last);
      }
      else{
         this->insert_equal (first, last);
      }
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_in_range_or_end(const_iterator pos) const
   {
      return (this->begin() <= pos) && (pos <= this->end());
   }

   // insert/erase
   void priv_insert_equal_prepare
      (const_iterator pos, const value_type& val, insert_commit_data &data)
   {
      // N1780
      //   To insert val at pos:
      //   if pos == end || val <= *pos
      //      if pos == begin || val >= *(pos-1)
      //         insert val before pos
      //      else
      //         insert val before upper_bound(val)
      //   else
      //      insert val before lower_bound(val)
      const value_compare &val_cmp = this->m_data;

      if(pos == this->cend() || !val_cmp(*pos, val)){
         if (pos == this->cbegin() || !val_cmp(val, pos[-1])){
            data.position = pos;
         }
         else{
            data.position =
               this->priv_upper_bound(this->cbegin(), pos, KeyOfValue()(val));
         }
      }
      else{
         data.position =
            this->priv_lower_bound(pos, this->cend(), KeyOfValue()(val));
      }
   }

   bool priv_insert_unique_prepare
      (const_iterator b, const_iterator e, const key_type& k, insert_commit_data &commit_data)
   {
      const key_compare &key_cmp  = this->priv_key_comp();
      commit_data.position = this->priv_lower_bound(b, e, k);
      return commit_data.position == e || key_cmp(k, KeyOfValue()(*commit_data.position));
   }

   BOOST_CONTAINER_FORCEINLINE bool priv_insert_unique_prepare
      (const key_type& k, insert_commit_data &commit_data)
   {  return this->priv_insert_unique_prepare(this->cbegin(), this->cend(), k, commit_data);   }

   bool priv_insert_unique_prepare
      (const_iterator pos, const key_type& k, insert_commit_data &commit_data)
   {
      //N1780. Props to Howard Hinnant!
      //To insert k at pos:
      //if pos == end || k <= *pos
      //   if pos == begin || k >= *(pos-1)
      //      insert k before pos
      //   else
      //      insert k before upper_bound(k)
      //else if pos+1 == end || k <= *(pos+1)
      //   insert k after pos
      //else
      //   insert k before lower_bound(k)
      const key_compare &key_cmp = this->priv_key_comp();
      const const_iterator cend_it = this->cend();
      if(pos == cend_it || key_cmp(k, KeyOfValue()(*pos))){ //Check if k should go before end
         const const_iterator cbeg = this->cbegin();
         commit_data.position = pos;
         if(pos == cbeg){  //If container is empty then insert it in the beginning
            return true;
         }
         const_iterator prev(pos);
         --prev;
         if(key_cmp(KeyOfValue()(*prev), k)){   //If previous element was less, then it should go between prev and pos
            return true;
         }
         else if(!key_cmp(k, KeyOfValue()(*prev))){   //If previous was equal then insertion should fail
            commit_data.position = prev;
            return false;
         }
         else{ //Previous was bigger so insertion hint was pointless, dispatch to hintless insertion
               //but reduce the search between beg and prev as prev is bigger than k
            return this->priv_insert_unique_prepare(cbeg, prev, k, commit_data);
         }
      }
      else{
         //The hint is before the insertion position, so insert it
         //in the remaining range [pos, end)
         return this->priv_insert_unique_prepare(pos, cend_it, k, commit_data);
      }
   }

   template<class Convertible>
   BOOST_CONTAINER_FORCEINLINE iterator priv_insert_commit
      (insert_commit_data &commit_data, BOOST_FWD_REF(Convertible) convertible)
   {
      return this->m_data.m_seq.insert
         ( commit_data.position
         , boost::forward<Convertible>(convertible));
   }

   template <class RanIt, class K>
   RanIt priv_lower_bound(RanIt first, const RanIt last,
                          const K & key) const
   {
      const Compare &key_cmp = this->m_data.get_comp();
      KeyOfValue key_extract;
      size_type len = static_cast<size_type>(last - first);
      RanIt middle;

      while (len) {
         size_type step = len >> 1;
         middle = first;
         middle += step;

         if (key_cmp(key_extract(*middle), key)) {
            first = ++middle;
            len -= step + 1;
         }
         else{
            len = step;
         }
      }
      return first;
   }

   template <class RanIt, class K>
   RanIt priv_upper_bound
      (RanIt first, const RanIt last,const K & key) const
   {
      const Compare &key_cmp = this->m_data.get_comp();
      KeyOfValue key_extract;
      size_type len = static_cast<size_type>(last - first);
      RanIt middle;

      while (len) {
         size_type step = len >> 1;
         middle = first;
         middle += step;

         if (key_cmp(key, key_extract(*middle))) {
            len = step;
         }
         else{
            first = ++middle;
            len -= step + 1;
         }
      }
      return first;
   }

   template <class RanIt, class K>
   std::pair<RanIt, RanIt>
      priv_equal_range(RanIt first, RanIt last, const K& key) const
   {
      const Compare &key_cmp = this->m_data.get_comp();
      KeyOfValue key_extract;
      size_type len = static_cast<size_type>(last - first);
      RanIt middle;

      while (len) {
         size_type step = len >> 1;
         middle = first;
         middle += step;

         if (key_cmp(key_extract(*middle), key)){
            first = ++middle;
            len -= step + 1;
         }
         else if (key_cmp(key, key_extract(*middle))){
            len = step;
         }
         else {
            //Middle is equal to key
            last = first;
            last += len;
            RanIt const first_ret = this->priv_lower_bound(first, middle, key);
            return std::pair<RanIt, RanIt>
               ( first_ret, this->priv_upper_bound(++middle, last, key));
         }
      }
      return std::pair<RanIt, RanIt>(first, first);
   }

   template<class RanIt, class K>
   std::pair<RanIt, RanIt> priv_lower_bound_range(RanIt first, RanIt last, const K& k) const
   {
      const Compare &key_cmp = this->m_data.get_comp();
      KeyOfValue key_extract;
      RanIt lb(this->priv_lower_bound(first, last, k)), ub(lb);
      if(lb != last && !key_cmp(k, key_extract(*lb))){
         ++ub;
      }
      return std::pair<RanIt, RanIt>(lb, ub);
   }
};

}  //namespace dtl {

}  //namespace container {

//!has_trivial_destructor_after_move<> == true_type
//!specialization for optimizations
template <class T, class KeyOfValue,
class Compare, class AllocatorOrContainer>
struct has_trivial_destructor_after_move<boost::container::dtl::flat_tree<T, KeyOfValue, Compare, AllocatorOrContainer> >
{
   typedef boost::container::dtl::flat_tree<T, KeyOfValue, Compare, AllocatorOrContainer> flat_tree;
   typedef typename flat_tree::container_type container_type;
   typedef typename flat_tree::key_compare key_compare;
   static const bool value = ::boost::has_trivial_destructor_after_move<container_type>::value &&
                             ::boost::has_trivial_destructor_after_move<key_compare>::value;
};

}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_FLAT_TREE_HPP
