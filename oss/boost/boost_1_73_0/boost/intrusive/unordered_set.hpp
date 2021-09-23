/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_INTRUSIVE_UNORDERED_SET_HPP
#define BOOST_INTRUSIVE_UNORDERED_SET_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/hashtable.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/static_assert.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

//! The class template unordered_set is an intrusive container, that mimics most of
//! the interface of std::tr1::unordered_set as described in the C++ TR1.
//!
//! unordered_set is a semi-intrusive container: each object to be stored in the
//! container must contain a proper hook, but the container also needs
//! additional auxiliary memory to work: unordered_set needs a pointer to an array
//! of type `bucket_type` to be passed in the constructor. This bucket array must
//! have at least the same lifetime as the container. This makes the use of
//! unordered_set more complicated than purely intrusive containers.
//! `bucket_type` is default-constructible, copyable and assignable
//!
//! The template parameter \c T is the type to be managed by the container.
//! The user can specify additional options and if no options are provided
//! default options are used.
//!
//! The container supports the following options:
//! \c base_hook<>/member_hook<>/value_traits<>,
//! \c constant_time_size<>, \c size_type<>, \c hash<> and \c equal<>
//! \c bucket_traits<>, \c power_2_buckets<> and \c cache_begin<>.
//!
//! unordered_set only provides forward iterators but it provides 4 iterator types:
//! iterator and const_iterator to navigate through the whole container and
//! local_iterator and const_local_iterator to navigate through the values
//! stored in a single bucket. Local iterators are faster and smaller.
//!
//! It's not recommended to use non constant-time size unordered_sets because several
//! key functions, like "empty()", become non-constant time functions. Non
//! constant-time size unordered_sets are mainly provided to support auto-unlink hooks.
//!
//! unordered_set, unlike std::unordered_set, does not make automatic rehashings nor
//! offers functions related to a load factor. Rehashing can be explicitly requested
//! and the user must provide a new bucket array that will be used from that moment.
//!
//! Since no automatic rehashing is done, iterators are never invalidated when
//! inserting or erasing elements. Iterators are only invalidated when rehasing.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class T, class ...Options>
#else
template<class ValueTraits, class VoidOrKeyOfValue, class VoidOrKeyHash, class VoidOrKeyEqual, class SizeType, class BucketTraits, std::size_t BoolFlags>
#endif
class unordered_set_impl
   : public hashtable_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyHash, VoidOrKeyEqual, BucketTraits, SizeType, BoolFlags|hash_bool_flags::unique_keys_pos>
{
   /// @cond
   private:
   typedef hashtable_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyHash, VoidOrKeyEqual, BucketTraits, SizeType, BoolFlags|hash_bool_flags::unique_keys_pos> table_type;

   template<class Iterator, class MaybeConstThis, class KeyType, class KeyHasher, class KeyEqual>
   static std::pair<Iterator,Iterator> priv_equal_range(MaybeConstThis &c, const KeyType& key, KeyHasher hash_func, KeyEqual equal_func)
   {
      Iterator const it = c.find(key, hash_func, equal_func);
      std::pair<Iterator,Iterator> ret(it, it);      
      if(it != c.end())
         ++ret.second;
      return ret;
   }

   //! This class is
   //! movable
   BOOST_MOVABLE_BUT_NOT_COPYABLE(unordered_set_impl)

   typedef table_type implementation_defined;
   /// @endcond

   public:
   typedef typename implementation_defined::value_type                  value_type;
   typedef typename implementation_defined::key_type                    key_type;
   typedef typename implementation_defined::key_of_value                key_of_value;
   typedef typename implementation_defined::value_traits                value_traits;
   typedef typename implementation_defined::bucket_traits               bucket_traits;
   typedef typename implementation_defined::pointer                     pointer;
   typedef typename implementation_defined::const_pointer               const_pointer;
   typedef typename implementation_defined::reference                   reference;
   typedef typename implementation_defined::const_reference             const_reference;
   typedef typename implementation_defined::difference_type             difference_type;
   typedef typename implementation_defined::size_type                   size_type;
   typedef typename implementation_defined::key_equal                   key_equal;
   typedef typename implementation_defined::hasher                      hasher;
   typedef typename implementation_defined::bucket_type                 bucket_type;
   typedef typename implementation_defined::bucket_ptr                  bucket_ptr;
   typedef typename implementation_defined::iterator                    iterator;
   typedef typename implementation_defined::const_iterator              const_iterator;
   typedef typename implementation_defined::insert_commit_data          insert_commit_data;
   typedef typename implementation_defined::local_iterator              local_iterator;
   typedef typename implementation_defined::const_local_iterator        const_local_iterator;
   typedef typename implementation_defined::node_traits                 node_traits;
   typedef typename implementation_defined::node                        node;
   typedef typename implementation_defined::node_ptr                    node_ptr;
   typedef typename implementation_defined::const_node_ptr              const_node_ptr;
   typedef typename implementation_defined::node_algorithms             node_algorithms;

   public:

   //! @copydoc ::boost::intrusive::hashtable::hashtable(const bucket_traits &,const hasher &,const key_equal &,const value_traits &)
   BOOST_INTRUSIVE_FORCEINLINE explicit unordered_set_impl( const bucket_traits &b_traits
                              , const hasher & hash_func = hasher()
                              , const key_equal &equal_func = key_equal()
                              , const value_traits &v_traits = value_traits())
      :  table_type(b_traits, hash_func, equal_func, v_traits)
   {}

   //! @copydoc ::boost::intrusive::hashtable::hashtable(bool,Iterator,Iterator,const bucket_traits &,const hasher &,const key_equal &,const value_traits &)
   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE unordered_set_impl( Iterator b
                     , Iterator e
                     , const bucket_traits &b_traits
                     , const hasher & hash_func = hasher()
                     , const key_equal &equal_func = key_equal()
                     , const value_traits &v_traits = value_traits())
      :  table_type(true, b, e, b_traits, hash_func, equal_func, v_traits)
   {}

   //! @copydoc ::boost::intrusive::hashtable::hashtable(hashtable&&)
   BOOST_INTRUSIVE_FORCEINLINE unordered_set_impl(BOOST_RV_REF(unordered_set_impl) x)
      :  table_type(BOOST_MOVE_BASE(table_type, x))
   {}

   //! @copydoc ::boost::intrusive::hashtable::operator=(hashtable&&)
   BOOST_INTRUSIVE_FORCEINLINE unordered_set_impl& operator=(BOOST_RV_REF(unordered_set_impl) x)
   {  return static_cast<unordered_set_impl&>(table_type::operator=(BOOST_MOVE_BASE(table_type, x))); }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::hashtable::~hashtable()
   ~unordered_set_impl();

   //! @copydoc ::boost::intrusive::hashtable::begin()
   iterator begin();

   //! @copydoc ::boost::intrusive::hashtable::begin()const
   const_iterator begin() const;

   //! @copydoc ::boost::intrusive::hashtable::cbegin()const
   const_iterator cbegin() const;

   //! @copydoc ::boost::intrusive::hashtable::end()
   iterator end();

   //! @copydoc ::boost::intrusive::hashtable::end()const
   const_iterator end() const;

   //! @copydoc ::boost::intrusive::hashtable::cend()const
   const_iterator cend() const;

   //! @copydoc ::boost::intrusive::hashtable::hash_function()const
   hasher hash_function() const;

   //! @copydoc ::boost::intrusive::hashtable::key_eq()const
   key_equal key_eq() const;

   //! @copydoc ::boost::intrusive::hashtable::empty()const
   bool empty() const;

   //! @copydoc ::boost::intrusive::hashtable::size()const
   size_type size() const;

   //! @copydoc ::boost::intrusive::hashtable::hashtable
   void swap(unordered_set_impl& other);

   //! @copydoc ::boost::intrusive::hashtable::clone_from(const hashtable&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(const unordered_set_impl &src, Cloner cloner, Disposer disposer);

   #else

   using table_type::clone_from;

   #endif //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::hashtable::clone_from(hashtable&&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(unordered_set_impl) src, Cloner cloner, Disposer disposer)
   {  table_type::clone_from(BOOST_MOVE_BASE(table_type, src), cloner, disposer);  }

   //! @copydoc ::boost::intrusive::hashtable::insert_unique(reference)
   BOOST_INTRUSIVE_FORCEINLINE std::pair<iterator, bool> insert(reference value)
   {  return table_type::insert_unique(value);  }

   //! @copydoc ::boost::intrusive::hashtable::insert_unique(Iterator,Iterator)
   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE void insert(Iterator b, Iterator e)
   {  table_type::insert_unique(b, e);  }

   //! @copydoc ::boost::intrusive::hashtable::insert_unique_check(const key_type&,insert_commit_data&)
   BOOST_INTRUSIVE_FORCEINLINE std::pair<iterator, bool> insert_check(const key_type &key, insert_commit_data &commit_data)
   {  return table_type::insert_unique_check(key, commit_data); }

   //! @copydoc ::boost::intrusive::hashtable::insert_unique_check(const KeyType&,KeyHasher,KeyEqual,insert_commit_data&)
   template<class KeyType, class KeyHasher, class KeyEqual>
   BOOST_INTRUSIVE_FORCEINLINE std::pair<iterator, bool> insert_check
      (const KeyType &key, KeyHasher hasher, KeyEqual key_value_equal, insert_commit_data &commit_data)
   {  return table_type::insert_unique_check(key, hasher, key_value_equal, commit_data); }

   //! @copydoc ::boost::intrusive::hashtable::insert_unique_commit
   BOOST_INTRUSIVE_FORCEINLINE iterator insert_commit(reference value, const insert_commit_data &commit_data)
   {  return table_type::insert_unique_commit(value, commit_data); }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::hashtable::erase(const_iterator)
   void erase(const_iterator i);

   //! @copydoc ::boost::intrusive::hashtable::erase(const_iterator,const_iterator)
   void erase(const_iterator b, const_iterator e);

   //! @copydoc ::boost::intrusive::hashtable::erase(const key_type &)
   size_type erase(const key_type &key);

   //! @copydoc ::boost::intrusive::hashtable::erase(const KeyType&,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   size_type erase(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const_iterator,Disposer)
   template<class Disposer>
   BOOST_INTRUSIVE_DOC1ST(void
      , typename detail::disable_if_convertible<Disposer BOOST_INTRUSIVE_I const_iterator>::type)
      erase_and_dispose(const_iterator i, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const_iterator,const_iterator,Disposer)
   template<class Disposer>
   void erase_and_dispose(const_iterator b, const_iterator e, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const key_type &,Disposer)
   template<class Disposer>
   size_type erase_and_dispose(const key_type &key, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const KeyType&,KeyHasher,KeyEqual,Disposer)
   template<class KeyType, class KeyHasher, class KeyEqual, class Disposer>
   size_type erase_and_dispose(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::clear
   void clear();

   //! @copydoc ::boost::intrusive::hashtable::clear_and_dispose
   template<class Disposer>
   void clear_and_dispose(Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::count(const key_type &)const
   size_type count(const key_type &key) const;

   //! @copydoc ::boost::intrusive::hashtable::count(const KeyType&,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   size_type count(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const;

   //! @copydoc ::boost::intrusive::hashtable::find(const key_type &)
   iterator find(const key_type &key);

   //! @copydoc ::boost::intrusive::hashtable::find(const KeyType &,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   iterator find(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func);

   //! @copydoc ::boost::intrusive::hashtable::count(const key_type &)const
   const_iterator find(const key_type &key) const;

   //! @copydoc ::boost::intrusive::hashtable::find(const KeyType &,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   const_iterator find(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const;
   #endif

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const key_type&)
   std::pair<iterator,iterator> equal_range(const key_type &key)
   {  return this->equal_range(key, this->hash_function(), this->key_eq());  }

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const KeyType &,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   std::pair<iterator,iterator> equal_range(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func)
   {  return this->priv_equal_range<iterator>(*this, key, hash_func, equal_func); }

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const key_type&)const
   std::pair<const_iterator, const_iterator>
      equal_range(const key_type &key) const
   {  return this->equal_range(key, this->hash_function(), this->key_eq());  }

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const KeyType &,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   std::pair<const_iterator, const_iterator>
      equal_range(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const
   {  return this->priv_equal_range<const_iterator>(*this, key, hash_func, equal_func); }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   //! @copydoc ::boost::intrusive::hashtable::iterator_to(reference)
   iterator iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::iterator_to(const_reference)const
   const_iterator iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::hashtable::s_local_iterator_to(reference)
   static local_iterator s_local_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::s_local_iterator_to(const_reference)
   static const_local_iterator s_local_iterator_to(const_reference value);

   //! @copydoc ::boost::intrusive::hashtable::local_iterator_to(reference)
   local_iterator local_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::local_iterator_to(const_reference)
   const_local_iterator local_iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_count
   size_type bucket_count() const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_size
   size_type bucket_size(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket(const key_type&)const
   size_type bucket(const key_type& k) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket(const KeyType&,KeyHasher)const
   template<class KeyType, class KeyHasher>
   size_type bucket(const KeyType& k,  KeyHasher hash_func) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_pointer
   bucket_ptr bucket_pointer() const;

   //! @copydoc ::boost::intrusive::hashtable::begin(size_type)
   local_iterator begin(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::begin(size_type)const
   const_local_iterator begin(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::cbegin(size_type)const
   const_local_iterator cbegin(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::end(size_type)
   local_iterator end(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::end(size_type)const
   const_local_iterator end(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::cend(size_type)const
   const_local_iterator cend(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::rehash(const bucket_traits &)
   void rehash(const bucket_traits &new_bucket_traits);

   //! @copydoc ::boost::intrusive::hashtable::full_rehash
   void full_rehash();

   //! @copydoc ::boost::intrusive::hashtable::incremental_rehash(bool)
   bool incremental_rehash(bool grow = true);

   //! @copydoc ::boost::intrusive::hashtable::incremental_rehash(const bucket_traits &)
   bool incremental_rehash(const bucket_traits &new_bucket_traits);

   //! @copydoc ::boost::intrusive::hashtable::split_count
   size_type split_count() const;

   //! @copydoc ::boost::intrusive::hashtable::suggested_upper_bucket_count
   static size_type suggested_upper_bucket_count(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::suggested_lower_bucket_count
   static size_type suggested_lower_bucket_count(size_type n);

   #endif   //   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   friend bool operator==(const unordered_set_impl &x, const unordered_set_impl &y)
   {
      if(table_type::constant_time_size && x.size() != y.size()){
         return false;
      }
      //Find each element of x in y
      for (const_iterator ix = x.cbegin(), ex = x.cend(), ey = y.cend(); ix != ex; ++ix){
         const_iterator iy = y.find(key_of_value()(*ix));
         if (iy == ey || !(*ix == *iy))
            return false;
      }
      return true;
   }

   friend bool operator!=(const unordered_set_impl &x, const unordered_set_impl &y)
   {  return !(x == y); }

   friend bool operator<(const unordered_set_impl &x, const unordered_set_impl &y)
   {  return ::boost::intrusive::algo_lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());  }

   friend bool operator>(const unordered_set_impl &x, const unordered_set_impl &y)
   {  return y < x;  }

   friend bool operator<=(const unordered_set_impl &x, const unordered_set_impl &y)
   {  return !(y < x);  }

   friend bool operator>=(const unordered_set_impl &x, const unordered_set_impl &y)
   {  return !(x < y);  }
};

//! Helper metafunction to define an \c unordered_set that yields to the same type when the
//! same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class ...Options>
#else
template<class T, class O1 = void, class O2 = void
                , class O3 = void, class O4 = void
                , class O5 = void, class O6 = void
                , class O7 = void, class O8 = void
                , class O9 = void, class O10= void
                >
#endif
struct make_unordered_set
{
   /// @cond
   typedef typename pack_options
      < hashtable_defaults,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type packed_options;

   typedef typename detail::get_value_traits
      <T, typename packed_options::proto_value_traits>::type value_traits;

   typedef typename make_bucket_traits
            <T, true, packed_options>::type bucket_traits;

   typedef unordered_set_impl
      < value_traits
      , typename packed_options::key_of_value
      , typename packed_options::hash
      , typename packed_options::equal
      , typename packed_options::size_type
      , bucket_traits
      ,  (std::size_t(true)*hash_bool_flags::unique_keys_pos)
      |  (std::size_t(packed_options::constant_time_size)*hash_bool_flags::constant_time_size_pos)
      |  (std::size_t(packed_options::power_2_buckets)*hash_bool_flags::power_2_buckets_pos)
      |  (std::size_t(packed_options::cache_begin)*hash_bool_flags::cache_begin_pos)
      |  (std::size_t(packed_options::compare_hash)*hash_bool_flags::compare_hash_pos)
      |  (std::size_t(packed_options::incremental)*hash_bool_flags::incremental_pos)
      > implementation_defined;

   /// @endcond
   typedef implementation_defined type;
};

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class O1, class O2, class O3, class O4, class O5, class O6, class O7, class O8, class O9, class O10>
#else
template<class T, class ...Options>
#endif
class unordered_set
   :  public make_unordered_set<T,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type
{
   typedef typename make_unordered_set
      <T,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type   Base;

   //Assert if passed value traits are compatible with the type
   BOOST_STATIC_ASSERT((detail::is_same<typename Base::value_traits::value_type, T>::value));
   BOOST_MOVABLE_BUT_NOT_COPYABLE(unordered_set)

   public:
   typedef typename Base::value_traits       value_traits;
   typedef typename Base::bucket_traits      bucket_traits;
   typedef typename Base::iterator           iterator;
   typedef typename Base::const_iterator     const_iterator;
   typedef typename Base::bucket_ptr         bucket_ptr;
   typedef typename Base::size_type          size_type;
   typedef typename Base::hasher             hasher;
   typedef typename Base::key_equal          key_equal;

   BOOST_INTRUSIVE_FORCEINLINE
   explicit unordered_set  ( const bucket_traits &b_traits
                           , const hasher & hash_func = hasher()
                           , const key_equal &equal_func = key_equal()
                           , const value_traits &v_traits = value_traits())
      :  Base(b_traits, hash_func, equal_func, v_traits)
   {}

   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE
   unordered_set
                  ( Iterator b, Iterator e
                  , const bucket_traits &b_traits
                  , const hasher & hash_func = hasher()
                  , const key_equal &equal_func = key_equal()
                  , const value_traits &v_traits = value_traits())
      :  Base(b, e, b_traits, hash_func, equal_func, v_traits)
   {}

   BOOST_INTRUSIVE_FORCEINLINE unordered_set(BOOST_RV_REF(unordered_set) x)
      :  Base(BOOST_MOVE_BASE(Base, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE unordered_set& operator=(BOOST_RV_REF(unordered_set) x)
   {  return static_cast<unordered_set&>(this->Base::operator=(BOOST_MOVE_BASE(Base, x)));  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(const unordered_set &src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(src, cloner, disposer);  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(unordered_set) src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(BOOST_MOVE_BASE(Base, src), cloner, disposer);  }
};

#endif


//! The class template unordered_multiset is an intrusive container, that mimics most of
//! the interface of std::tr1::unordered_multiset as described in the C++ TR1.
//!
//! unordered_multiset is a semi-intrusive container: each object to be stored in the
//! container must contain a proper hook, but the container also needs
//! additional auxiliary memory to work: unordered_multiset needs a pointer to an array
//! of type `bucket_type` to be passed in the constructor. This bucket array must
//! have at least the same lifetime as the container. This makes the use of
//! unordered_multiset more complicated than purely intrusive containers.
//! `bucket_type` is default-constructible, copyable and assignable
//!
//! The template parameter \c T is the type to be managed by the container.
//! The user can specify additional options and if no options are provided
//! default options are used.
//!
//! The container supports the following options:
//! \c base_hook<>/member_hook<>/value_traits<>,
//! \c constant_time_size<>, \c size_type<>, \c hash<> and \c equal<>
//! \c bucket_traits<>, \c power_2_buckets<> and \c cache_begin<>.
//!
//! unordered_multiset only provides forward iterators but it provides 4 iterator types:
//! iterator and const_iterator to navigate through the whole container and
//! local_iterator and const_local_iterator to navigate through the values
//! stored in a single bucket. Local iterators are faster and smaller.
//!
//! It's not recommended to use non constant-time size unordered_multisets because several
//! key functions, like "empty()", become non-constant time functions. Non
//! constant-time size unordered_multisets are mainly provided to support auto-unlink hooks.
//!
//! unordered_multiset, unlike std::unordered_set, does not make automatic rehashings nor
//! offers functions related to a load factor. Rehashing can be explicitly requested
//! and the user must provide a new bucket array that will be used from that moment.
//!
//! Since no automatic rehashing is done, iterators are never invalidated when
//! inserting or erasing elements. Iterators are only invalidated when rehasing.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
template<class T, class ...Options>
#else
template<class ValueTraits, class VoidOrKeyOfValue, class VoidOrKeyHash, class VoidOrKeyEqual, class SizeType, class BucketTraits, std::size_t BoolFlags>
#endif
class unordered_multiset_impl
   : public hashtable_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyHash, VoidOrKeyEqual, BucketTraits, SizeType, BoolFlags>
{
   /// @cond
   private:
   typedef hashtable_impl<ValueTraits, VoidOrKeyOfValue, VoidOrKeyHash, VoidOrKeyEqual, BucketTraits, SizeType, BoolFlags> table_type;
   /// @endcond

   //Movable
   BOOST_MOVABLE_BUT_NOT_COPYABLE(unordered_multiset_impl)

   typedef table_type implementation_defined;

   public:
   typedef typename implementation_defined::value_type                  value_type;
   typedef typename implementation_defined::key_type                    key_type;
   typedef typename implementation_defined::value_traits                value_traits;
   typedef typename implementation_defined::bucket_traits               bucket_traits;
   typedef typename implementation_defined::pointer                     pointer;
   typedef typename implementation_defined::const_pointer               const_pointer;
   typedef typename implementation_defined::reference                   reference;
   typedef typename implementation_defined::const_reference             const_reference;
   typedef typename implementation_defined::difference_type             difference_type;
   typedef typename implementation_defined::size_type                   size_type;
   typedef typename implementation_defined::key_equal                   key_equal;
   typedef typename implementation_defined::hasher                      hasher;
   typedef typename implementation_defined::bucket_type                 bucket_type;
   typedef typename implementation_defined::bucket_ptr                  bucket_ptr;
   typedef typename implementation_defined::iterator                    iterator;
   typedef typename implementation_defined::const_iterator              const_iterator;
   typedef typename implementation_defined::insert_commit_data          insert_commit_data;
   typedef typename implementation_defined::local_iterator              local_iterator;
   typedef typename implementation_defined::const_local_iterator        const_local_iterator;
   typedef typename implementation_defined::node_traits                 node_traits;
   typedef typename implementation_defined::node                        node;
   typedef typename implementation_defined::node_ptr                    node_ptr;
   typedef typename implementation_defined::const_node_ptr              const_node_ptr;
   typedef typename implementation_defined::node_algorithms             node_algorithms;

   public:

   //! @copydoc ::boost::intrusive::hashtable::hashtable(const bucket_traits &,const hasher &,const key_equal &,const value_traits &)
   BOOST_INTRUSIVE_FORCEINLINE explicit unordered_multiset_impl ( const bucket_traits &b_traits
                                    , const hasher & hash_func = hasher()
                                    , const key_equal &equal_func = key_equal()
                                    , const value_traits &v_traits = value_traits())
      :  table_type(b_traits, hash_func, equal_func, v_traits)
   {}

   //! @copydoc ::boost::intrusive::hashtable::hashtable(bool,Iterator,Iterator,const bucket_traits &,const hasher &,const key_equal &,const value_traits &)
   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE unordered_multiset_impl ( Iterator b
                           , Iterator e
                           , const bucket_traits &b_traits
                           , const hasher & hash_func = hasher()
                           , const key_equal &equal_func = key_equal()
                           , const value_traits &v_traits = value_traits())
      :  table_type(false, b, e, b_traits, hash_func, equal_func, v_traits)
   {}

   //! <b>Effects</b>: to-do
   //!
   BOOST_INTRUSIVE_FORCEINLINE unordered_multiset_impl(BOOST_RV_REF(unordered_multiset_impl) x)
      :  table_type(BOOST_MOVE_BASE(table_type, x))
   {}

   //! <b>Effects</b>: to-do
   //!
   BOOST_INTRUSIVE_FORCEINLINE unordered_multiset_impl& operator=(BOOST_RV_REF(unordered_multiset_impl) x)
   {  return static_cast<unordered_multiset_impl&>(table_type::operator=(BOOST_MOVE_BASE(table_type, x)));  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::hashtable::~hashtable()
   ~unordered_multiset_impl();

   //! @copydoc ::boost::intrusive::hashtable::begin()
   iterator begin();

   //! @copydoc ::boost::intrusive::hashtable::begin()const
   const_iterator begin() const;

   //! @copydoc ::boost::intrusive::hashtable::cbegin()const
   const_iterator cbegin() const;

   //! @copydoc ::boost::intrusive::hashtable::end()
   iterator end();

   //! @copydoc ::boost::intrusive::hashtable::end()const
   const_iterator end() const;

   //! @copydoc ::boost::intrusive::hashtable::cend()const
   const_iterator cend() const;

   //! @copydoc ::boost::intrusive::hashtable::hash_function()const
   hasher hash_function() const;

   //! @copydoc ::boost::intrusive::hashtable::key_eq()const
   key_equal key_eq() const;

   //! @copydoc ::boost::intrusive::hashtable::empty()const
   bool empty() const;

   //! @copydoc ::boost::intrusive::hashtable::size()const
   size_type size() const;

   //! @copydoc ::boost::intrusive::hashtable::hashtable
   void swap(unordered_multiset_impl& other);

   //! @copydoc ::boost::intrusive::hashtable::clone_from(const hashtable&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   void clone_from(const unordered_multiset_impl &src, Cloner cloner, Disposer disposer);

   #else

   using table_type::clone_from;

   #endif   //   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::hashtable::clone_from(hashtable&&,Cloner,Disposer)
   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(unordered_multiset_impl) src, Cloner cloner, Disposer disposer)
   {  table_type::clone_from(BOOST_MOVE_BASE(table_type, src), cloner, disposer);  }

   //! @copydoc ::boost::intrusive::hashtable::insert_equal(reference)
   BOOST_INTRUSIVE_FORCEINLINE iterator insert(reference value)
   {  return table_type::insert_equal(value);  }

   //! @copydoc ::boost::intrusive::hashtable::insert_equal(Iterator,Iterator)
   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE void insert(Iterator b, Iterator e)
   {  table_type::insert_equal(b, e);  }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::hashtable::erase(const_iterator)
   void erase(const_iterator i);

   //! @copydoc ::boost::intrusive::hashtable::erase(const_iterator,const_iterator)
   void erase(const_iterator b, const_iterator e);

   //! @copydoc ::boost::intrusive::hashtable::erase(const key_type &)
   size_type erase(const key_type &key);

   //! @copydoc ::boost::intrusive::hashtable::erase(const KeyType&,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   size_type erase(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const_iterator,Disposer)
   template<class Disposer>
   BOOST_INTRUSIVE_DOC1ST(void
      , typename detail::disable_if_convertible<Disposer BOOST_INTRUSIVE_I const_iterator>::type)
      erase_and_dispose(const_iterator i, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const_iterator,const_iterator,Disposer)
   template<class Disposer>
   void erase_and_dispose(const_iterator b, const_iterator e, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const key_type &,Disposer)
   template<class Disposer>
   size_type erase_and_dispose(const key_type &key, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::erase_and_dispose(const KeyType&,KeyHasher,KeyEqual,Disposer)
   template<class KeyType, class KeyHasher, class KeyEqual, class Disposer>
   size_type erase_and_dispose(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func, Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::clear
   void clear();

   //! @copydoc ::boost::intrusive::hashtable::clear_and_dispose
   template<class Disposer>
   void clear_and_dispose(Disposer disposer);

   //! @copydoc ::boost::intrusive::hashtable::count(const key_type &)const
   size_type count(const key_type &key) const;

   //! @copydoc ::boost::intrusive::hashtable::count(const KeyType&,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   size_type count(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const;

   //! @copydoc ::boost::intrusive::hashtable::find(const key_type &)
   iterator find(const key_type &key);

   //! @copydoc ::boost::intrusive::hashtable::find(const KeyType &,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   iterator find(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func);

   //! @copydoc ::boost::intrusive::hashtable::count(const key_type &)const
   const_iterator find(const key_type &key) const;

   //! @copydoc ::boost::intrusive::hashtable::find(const KeyType &,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   const_iterator find(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const;

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const key_type&)
   std::pair<iterator,iterator> equal_range(const key_type &key);

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const KeyType &,KeyHasher,KeyEqual)
   template<class KeyType, class KeyHasher, class KeyEqual>
   std::pair<iterator,iterator> equal_range(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func);

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const key_type&)const
   std::pair<const_iterator, const_iterator>
      equal_range(const key_type &key) const;

   //! @copydoc ::boost::intrusive::hashtable::equal_range(const KeyType &,KeyHasher,KeyEqual)const
   template<class KeyType, class KeyHasher, class KeyEqual>
   std::pair<const_iterator, const_iterator>
      equal_range(const KeyType& key, KeyHasher hash_func, KeyEqual equal_func) const;

   //! @copydoc ::boost::intrusive::hashtable::iterator_to(reference)
   iterator iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::iterator_to(const_reference)const
   const_iterator iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::hashtable::s_local_iterator_to(reference)
   static local_iterator s_local_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::s_local_iterator_to(const_reference)
   static const_local_iterator s_local_iterator_to(const_reference value);

   //! @copydoc ::boost::intrusive::hashtable::local_iterator_to(reference)
   local_iterator local_iterator_to(reference value);

   //! @copydoc ::boost::intrusive::hashtable::local_iterator_to(const_reference)
   const_local_iterator local_iterator_to(const_reference value) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_count
   size_type bucket_count() const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_size
   size_type bucket_size(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket(const key_type&)const
   size_type bucket(const key_type& k) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket(const KeyType&,KeyHasher)const
   template<class KeyType, class KeyHasher>
   size_type bucket(const KeyType& k, KeyHasher hash_func) const;

   //! @copydoc ::boost::intrusive::hashtable::bucket_pointer
   bucket_ptr bucket_pointer() const;

   //! @copydoc ::boost::intrusive::hashtable::begin(size_type)
   local_iterator begin(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::begin(size_type)const
   const_local_iterator begin(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::cbegin(size_type)const
   const_local_iterator cbegin(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::end(size_type)
   local_iterator end(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::end(size_type)const
   const_local_iterator end(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::cend(size_type)const
   const_local_iterator cend(size_type n) const;

   //! @copydoc ::boost::intrusive::hashtable::rehash(const bucket_traits &)
   void rehash(const bucket_traits &new_bucket_traits);

   //! @copydoc ::boost::intrusive::hashtable::full_rehash
   void full_rehash();

   //! @copydoc ::boost::intrusive::hashtable::incremental_rehash(bool)
   bool incremental_rehash(bool grow = true);

   //! @copydoc ::boost::intrusive::hashtable::incremental_rehash(const bucket_traits &)
   bool incremental_rehash(const bucket_traits &new_bucket_traits);

   //! @copydoc ::boost::intrusive::hashtable::split_count
   size_type split_count() const;

   //! @copydoc ::boost::intrusive::hashtable::suggested_upper_bucket_count
   static size_type suggested_upper_bucket_count(size_type n);

   //! @copydoc ::boost::intrusive::hashtable::suggested_lower_bucket_count
   static size_type suggested_lower_bucket_count(size_type n);

   #endif   //   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
};

//! Helper metafunction to define an \c unordered_multiset that yields to the same type when the
//! same options (either explicitly or implicitly) are used.
#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED) || defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class ...Options>
#else
template<class T, class O1 = void, class O2 = void
                , class O3 = void, class O4 = void
                , class O5 = void, class O6 = void
                , class O7 = void, class O8 = void
                , class O9 = void, class O10= void
                >
#endif
struct make_unordered_multiset
{
   /// @cond
   typedef typename pack_options
      < hashtable_defaults,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type packed_options;

   typedef typename detail::get_value_traits
      <T, typename packed_options::proto_value_traits>::type value_traits;

   typedef typename make_bucket_traits
            <T, true, packed_options>::type bucket_traits;

   typedef unordered_multiset_impl
      < value_traits
      , typename packed_options::key_of_value
      , typename packed_options::hash
      , typename packed_options::equal
      , typename packed_options::size_type
      , bucket_traits
      ,  (std::size_t(false)*hash_bool_flags::unique_keys_pos)
      |  (std::size_t(packed_options::constant_time_size)*hash_bool_flags::constant_time_size_pos)
      |  (std::size_t(packed_options::power_2_buckets)*hash_bool_flags::power_2_buckets_pos)
      |  (std::size_t(packed_options::cache_begin)*hash_bool_flags::cache_begin_pos)
      |  (std::size_t(packed_options::compare_hash)*hash_bool_flags::compare_hash_pos)
      |  (std::size_t(packed_options::incremental)*hash_bool_flags::incremental_pos)
      > implementation_defined;

   /// @endcond
   typedef implementation_defined type;
};

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

#if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
template<class T, class O1, class O2, class O3, class O4, class O5, class O6, class O7, class O8, class O9, class O10>
#else
template<class T, class ...Options>
#endif
class unordered_multiset
   :  public make_unordered_multiset<T,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type
{
   typedef typename make_unordered_multiset
      <T,
         #if !defined(BOOST_INTRUSIVE_VARIADIC_TEMPLATES)
         O1, O2, O3, O4, O5, O6, O7, O8, O9, O10
         #else
         Options...
         #endif
      >::type   Base;
   //Assert if passed value traits are compatible with the type
   BOOST_STATIC_ASSERT((detail::is_same<typename Base::value_traits::value_type, T>::value));
   BOOST_MOVABLE_BUT_NOT_COPYABLE(unordered_multiset)

   public:
   typedef typename Base::value_traits       value_traits;
   typedef typename Base::bucket_traits      bucket_traits;
   typedef typename Base::iterator           iterator;
   typedef typename Base::const_iterator     const_iterator;
   typedef typename Base::bucket_ptr         bucket_ptr;
   typedef typename Base::size_type          size_type;
   typedef typename Base::hasher             hasher;
   typedef typename Base::key_equal          key_equal;

   BOOST_INTRUSIVE_FORCEINLINE
   explicit unordered_multiset( const bucket_traits &b_traits
                              , const hasher & hash_func = hasher()
                              , const key_equal &equal_func = key_equal()
                              , const value_traits &v_traits = value_traits())
      :  Base(b_traits, hash_func, equal_func, v_traits)
   {}

   template<class Iterator>
   BOOST_INTRUSIVE_FORCEINLINE
   unordered_multiset( Iterator b
                     , Iterator e
                     , const bucket_traits &b_traits
                     , const hasher & hash_func = hasher()
                     , const key_equal &equal_func = key_equal()
                     , const value_traits &v_traits = value_traits())
      :  Base(b, e, b_traits, hash_func, equal_func, v_traits)
   {}

   BOOST_INTRUSIVE_FORCEINLINE unordered_multiset(BOOST_RV_REF(unordered_multiset) x)
      :  Base(BOOST_MOVE_BASE(Base, x))
   {}

   BOOST_INTRUSIVE_FORCEINLINE unordered_multiset& operator=(BOOST_RV_REF(unordered_multiset) x)
   {  return static_cast<unordered_multiset&>(this->Base::operator=(BOOST_MOVE_BASE(Base, x)));  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(const unordered_multiset &src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(src, cloner, disposer);  }

   template <class Cloner, class Disposer>
   BOOST_INTRUSIVE_FORCEINLINE void clone_from(BOOST_RV_REF(unordered_multiset) src, Cloner cloner, Disposer disposer)
   {  Base::clone_from(BOOST_MOVE_BASE(Base, src), cloner, disposer);  }
};

#endif

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_UNORDERED_SET_HPP
