// Boost.Container static_vector
//
// Copyright (c) 2012-2013 Adam Wulkiewicz, Lodz, Poland.
// Copyright (c) 2011-2013 Andrew Hundt.
// Copyright (c) 2013-2014 Ion Gaztanaga
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONTAINER_STATIC_VECTOR_HPP
#define BOOST_CONTAINER_STATIC_VECTOR_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/container/vector.hpp>

#include <cstddef>
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
#include <initializer_list>
#endif

namespace boost { namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

namespace dtl {

template<class T, std::size_t N, std::size_t InplaceAlignment, bool ThrowOnOverflow>
class static_storage_allocator
{
   typedef bool_<ThrowOnOverflow> throw_on_overflow_t;

   static BOOST_NORETURN BOOST_CONTAINER_FORCEINLINE void on_capacity_overflow(true_type)
   {
      (throw_bad_alloc)();
   }

   static BOOST_CONTAINER_FORCEINLINE void on_capacity_overflow(false_type)
   {
      BOOST_ASSERT_MSG(false, "ERROR: static vector capacity overflow");
   }
   
   public:
   typedef T value_type;

   BOOST_CONTAINER_FORCEINLINE static_storage_allocator() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   BOOST_CONTAINER_FORCEINLINE static_storage_allocator(const static_storage_allocator &) BOOST_NOEXCEPT_OR_NOTHROW
   {}

   BOOST_CONTAINER_FORCEINLINE static_storage_allocator & operator=(const static_storage_allocator &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return *this;  }

   BOOST_CONTAINER_FORCEINLINE T* internal_storage() const BOOST_NOEXCEPT_OR_NOTHROW
   {  return const_cast<T*>(static_cast<const T*>(static_cast<const void*>(storage.data)));  }

   BOOST_CONTAINER_FORCEINLINE T* internal_storage() BOOST_NOEXCEPT_OR_NOTHROW
   {  return static_cast<T*>(static_cast<void*>(storage.data));  }

   static const std::size_t internal_capacity = N;

   std::size_t max_size() const
   {  return N;   }

   static BOOST_CONTAINER_FORCEINLINE void on_capacity_overflow()
   {
      (on_capacity_overflow)(throw_on_overflow_t());
   }

   typedef boost::container::dtl::version_type<static_storage_allocator, 0>   version;

   BOOST_CONTAINER_FORCEINLINE friend bool operator==(const static_storage_allocator &, const static_storage_allocator &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return false;  }

   BOOST_CONTAINER_FORCEINLINE friend bool operator!=(const static_storage_allocator &, const static_storage_allocator &) BOOST_NOEXCEPT_OR_NOTHROW
   {  return true;  }

   private:
   BOOST_STATIC_ASSERT_MSG(!InplaceAlignment || (InplaceAlignment & (InplaceAlignment-1)) == 0, "Alignment option must be zero or power of two");
   static const std::size_t final_alignment = InplaceAlignment ? InplaceAlignment : dtl::alignment_of<T>::value;
   typename dtl::aligned_storage<sizeof(T)*N, final_alignment>::type storage;
};

template<class Options>
struct get_static_vector_opt
{
   typedef Options type;
};

template<>
struct get_static_vector_opt<void>
{
   typedef static_vector_null_opt type;
};

template <typename T, std::size_t Capacity, class Options>
struct get_static_vector_allocator
{
   typedef typename  get_static_vector_opt<Options>::type options_t;
   typedef dtl::static_storage_allocator
      < T
      , Capacity
      , options_t::inplace_alignment
      , options_t::throw_on_overflow
      > type;
};


}  //namespace dtl {

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

//!
//!@brief A variable-size array container with fixed capacity.
//!
//!static_vector is a sequence container like boost::container::vector with contiguous storage that can
//!change in size, along with the static allocation, low overhead, and fixed capacity of boost::array.
//!
//!A static_vector is a sequence that supports random access to elements, constant time insertion and
//!removal of elements at the end, and linear time insertion and removal of elements at the beginning or
//!in the middle. The number of elements in a static_vector may vary dynamically up to a fixed capacity
//!because elements are stored within the object itself similarly to an array. However, objects are
//!initialized as they are inserted into static_vector unlike C arrays or std::array which must construct
//!all elements on instantiation. The behavior of static_vector enables the use of statically allocated
//!elements in cases with complex object lifetime requirements that would otherwise not be trivially
//!possible.
//!
//!@par Error Handling
//! Insertion beyond the capacity result in throwing std::bad_alloc() if exceptions are enabled or
//! calling throw_bad_alloc() if not enabled.
//!
//! std::out_of_range is thrown if out of bounds access is performed in <code>at()</code> if exceptions are
//! enabled, throw_out_of_range() if not enabled.
//!
//!@tparam T    The type of element that will be stored.
//!@tparam Capacity The maximum number of elements static_vector can store, fixed at compile time.
//!@tparam Options A type produced from \c boost::container::static_vector_options.
template <typename T, std::size_t Capacity, class Options BOOST_CONTAINER_DOCONLY(= void) >
class static_vector
    : public vector<T, typename dtl::get_static_vector_allocator< T, Capacity, Options>::type>
{
   public:
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   typedef typename dtl::get_static_vector_allocator< T, Capacity, Options>::type allocator_type;
   typedef vector<T, allocator_type > base_t;

   BOOST_COPYABLE_AND_MOVABLE(static_vector)

   template<class U, std::size_t OtherCapacity, class OtherOptions>
   friend class static_vector;

   public:
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

public:
    //! @brief The type of elements stored in the container.
    typedef typename base_t::value_type value_type;
    //! @brief The unsigned integral type used by the container.
    typedef typename base_t::size_type size_type;
    //! @brief The pointers difference type.
    typedef typename base_t::difference_type difference_type;
    //! @brief The pointer type.
    typedef typename base_t::pointer pointer;
    //! @brief The const pointer type.
    typedef typename base_t::const_pointer const_pointer;
    //! @brief The value reference type.
    typedef typename base_t::reference reference;
    //! @brief The value const reference type.
    typedef typename base_t::const_reference const_reference;
    //! @brief The iterator type.
    typedef typename base_t::iterator iterator;
    //! @brief The const iterator type.
    typedef typename base_t::const_iterator const_iterator;
    //! @brief The reverse iterator type.
    typedef typename base_t::reverse_iterator reverse_iterator;
    //! @brief The const reverse iterator.
    typedef typename base_t::const_reverse_iterator const_reverse_iterator;

    //! @brief The capacity/max size of the container
    static const size_type static_capacity = Capacity;

    //! @brief Constructs an empty static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    BOOST_CONTAINER_FORCEINLINE static_vector() BOOST_NOEXCEPT_OR_NOTHROW
        : base_t()
    {}

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Constructs a static_vector containing count value initialized values.
    //!
    //! @param count    The number of values which will be contained in the container.
    //!
    //! @par Throws
    //!   If T's value initialization throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE explicit static_vector(size_type count)
        : base_t(count)
    {}

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Constructs a static_vector containing count default initialized values.
    //!
    //! @param count    The number of values which will be contained in the container.
    //!
    //! @par Throws
    //!   If T's default initialization throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    //!
    //! @par Note
    //!   Non-standard extension
    BOOST_CONTAINER_FORCEINLINE static_vector(size_type count, default_init_t)
        : base_t(count, default_init_t())
    {}

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Constructs a static_vector containing count copies of value.
    //!
    //! @param count    The number of copies of a values that will be contained in the container.
    //! @param value    The value which will be used to copy construct values.
    //!
    //! @par Throws
    //!   If T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector(size_type count, value_type const& value)
        : base_t(count, value)
    {}

    //! @pre
    //!  @li <tt>distance(first, last) <= capacity()</tt>
    //!  @li Iterator must meet the \c ForwardTraversalIterator concept.
    //!
    //! @brief Constructs a static_vector containing copy of a range <tt>[first, last)</tt>.
    //!
    //! @param first    The iterator to the first element in range.
    //! @param last     The iterator to the one after the last element in range.
    //!
    //! @par Throws
    //!   If T's constructor taking a dereferenced Iterator throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <typename Iterator>
    BOOST_CONTAINER_FORCEINLINE static_vector(Iterator first, Iterator last)
        : base_t(first, last)
    {}

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
    //! @pre
    //!  @li <tt>distance(il.begin(), il.end()) <= capacity()</tt>
    //!
    //! @brief Constructs a static_vector containing copy of a range <tt>[il.begin(), il.end())</tt>.
    //!
    //! @param il       std::initializer_list with values to initialize vector.
    //!
    //! @par Throws
    //!   If T's constructor taking a dereferenced std::initializer_list throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector(std::initializer_list<value_type> il)
        : base_t(il)
    {}
#endif

    //! @brief Constructs a copy of other static_vector.
    //!
    //! @param other    The static_vector which content will be copied to this one.
    //!
    //! @par Throws
    //!   If T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector(static_vector const& other)
        : base_t(other)
    {}

    BOOST_CONTAINER_FORCEINLINE static_vector(static_vector const& other, const allocator_type &)
       : base_t(other)
    {}

    BOOST_CONTAINER_FORCEINLINE static_vector(BOOST_RV_REF(static_vector) other,  const allocator_type &)
       BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_constructible<value_type>::value)
       : base_t(BOOST_MOVE_BASE(base_t, other))
    {}

    BOOST_CONTAINER_FORCEINLINE explicit static_vector(const allocator_type &)
       : base_t()
    {}

    //! @pre <tt>other.size() <= capacity()</tt>.
    //!
    //! @brief Constructs a copy of other static_vector.
    //!
    //! @param other    The static_vector which content will be copied to this one.
    //!
    //! @par Throws
    //!   If T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <std::size_t C, class O>
    BOOST_CONTAINER_FORCEINLINE static_vector(static_vector<T, C, O> const& other)
        : base_t(other)
    {}

    //! @brief Move constructor. Moves Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be moved to this one.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor throws.
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector(BOOST_RV_REF(static_vector) other)
      BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_constructible<value_type>::value)
        : base_t(BOOST_MOVE_BASE(base_t, other))
    {}

    //! @pre <tt>other.size() <= capacity()</tt>
    //!
    //! @brief Move constructor. Moves Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be moved to this one.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor throws.
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <std::size_t C, class O>
    BOOST_CONTAINER_FORCEINLINE static_vector(BOOST_RV_REF_BEG static_vector<T, C, O> BOOST_RV_REF_END other)
        : base_t(BOOST_MOVE_BASE(typename static_vector<T BOOST_MOVE_I C>::base_t, other))
    {}

    //! @brief Copy assigns Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be copied to this one.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //! Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector & operator=(BOOST_COPY_ASSIGN_REF(static_vector) other)
    {
        return static_cast<static_vector&>(base_t::operator=(static_cast<base_t const&>(other)));
    }

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
    //! @brief Copy assigns Values stored in std::initializer_list to *this.
    //!
    //! @param il    The std::initializer_list which content will be copied to this one.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //! Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector & operator=(std::initializer_list<value_type> il)
    { return static_cast<static_vector&>(base_t::operator=(il));  }
#endif

    //! @pre <tt>other.size() <= capacity()</tt>
    //!
    //! @brief Copy assigns Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be copied to this one.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <std::size_t C, class O>
    BOOST_CONTAINER_FORCEINLINE static_vector & operator=(static_vector<T, C, O> const& other)
    {
        return static_cast<static_vector&>(base_t::operator=
            (static_cast<typename static_vector<T, C, O>::base_t const&>(other)));
    }

    //! @brief Move assignment. Moves Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be moved to this one.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor or move assignment throws.
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    BOOST_CONTAINER_FORCEINLINE static_vector & operator=(BOOST_RV_REF(static_vector) other)
       BOOST_NOEXCEPT_IF(boost::container::dtl::is_nothrow_move_assignable<value_type>::value)
    {
        return static_cast<static_vector&>(base_t::operator=(BOOST_MOVE_BASE(base_t, other)));
    }

    //! @pre <tt>other.size() <= capacity()</tt>
    //!
    //! @brief Move assignment. Moves Values stored in the other static_vector to this one.
    //!
    //! @param other    The static_vector which content will be moved to this one.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor or move assignment throws.
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <std::size_t C, class O>
    BOOST_CONTAINER_FORCEINLINE static_vector & operator=(BOOST_RV_REF_BEG static_vector<T, C, O> BOOST_RV_REF_END other)
    {
        return static_cast<static_vector&>(base_t::operator=
         (BOOST_MOVE_BASE(typename static_vector<T BOOST_MOVE_I C>::base_t, other)));
    }

#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

    //! @brief Destructor. Destroys Values stored in this container.
    //!
    //! @par Throws
    //!   Nothing
    //!
    //! @par Complexity
    //!   Linear O(N).
    ~static_vector();

    //! @brief Swaps contents of the other static_vector and this one.
    //!
    //! @param other    The static_vector which content will be swapped with this one's content.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor or move assignment throws,
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor or copy assignment throws,
    //!
    //! @par Complexity
    //!   Linear O(N).
    void swap(static_vector & other);

    //! @pre <tt>other.size() <= capacity() && size() <= other.capacity()</tt>
    //!
    //! @brief Swaps contents of the other static_vector and this one.
    //!
    //! @param other    The static_vector which content will be swapped with this one's content.
    //!
    //! @par Throws
    //!   @li If \c has_nothrow_move<T>::value is \c true and T's move constructor or move assignment throws,
    //!   @li If \c has_nothrow_move<T>::value is \c false and T's copy constructor or copy assignment throws,
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <std::size_t C, class O>
    void swap(static_vector<T, C, O> & other);

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Inserts or erases elements at the end such that
    //!   the size becomes count. New elements are value initialized.
    //!
    //! @param count    The number of elements which will be stored in the container.
    //!
    //! @par Throws
    //!   If T's value initialization throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    void resize(size_type count);

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Inserts or erases elements at the end such that
    //!   the size becomes count. New elements are default initialized.
    //!
    //! @param count    The number of elements which will be stored in the container.
    //!
    //! @par Throws
    //!   If T's default initialization throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    //!
    //! @par Note
    //!   Non-standard extension
    void resize(size_type count, default_init_t);

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Inserts or erases elements at the end such that
    //!   the size becomes count. New elements are copy constructed from value.
    //!
    //! @param count    The number of elements which will be stored in the container.
    //! @param value    The value used to copy construct the new element.
    //!
    //! @par Throws
    //!   If T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    void resize(size_type count, value_type const& value);

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief This call has no effect because the Capacity of this container is constant.
    //!
    //! @param count    The number of elements which the container should be able to contain.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Linear O(N).
    void reserve(size_type count)  BOOST_NOEXCEPT_OR_NOTHROW;

    //! @pre <tt>size() < capacity()</tt>
    //!
    //! @brief Adds a copy of value at the end.
    //!
    //! @param value    The value used to copy construct the new element.
    //!
    //! @par Throws
    //!   If T's copy constructor throws.
    //!
    //! @par Complexity
    //!   Constant O(1).
    void push_back(value_type const& value);

    //! @pre <tt>size() < capacity()</tt>
    //!
    //! @brief Moves value to the end.
    //!
    //! @param value    The value to move construct the new element.
    //!
    //! @par Throws
    //!   If T's move constructor throws.
    //!
    //! @par Complexity
    //!   Constant O(1).
    void push_back(BOOST_RV_REF(value_type) value);

    //! @pre <tt>!empty()</tt>
    //!
    //! @brief Destroys last value and decreases the size.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    void pop_back();

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>.
    //!  @li <tt>size() < capacity()</tt>
    //!
    //! @brief Inserts a copy of element at p.
    //!
    //! @param p     The position at which the new value will be inserted.
    //! @param value The value used to copy construct the new element.
    //!
    //! @par Throws
    //!   @li If T's copy constructor or copy assignment throws
    //!   @li If T's move constructor or move assignment throws.
    //!
    //! @par Complexity
    //!   Constant or linear.
    iterator insert(const_iterator p, value_type const& value);

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>.
    //!  @li <tt>size() < capacity()</tt>
    //!
    //! @brief Inserts a move-constructed element at p.
    //!
    //! @param p     The position at which the new value will be inserted.
    //! @param value The value used to move construct the new element.
    //!
    //! @par Throws
    //!   If T's move constructor or move assignment throws.
    //!
    //! @par Complexity
    //!   Constant or linear.
    iterator insert(const_iterator p, BOOST_RV_REF(value_type) value);

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>.
    //!  @li <tt>size() + count <= capacity()</tt>
    //!
    //! @brief Inserts a count copies of value at p.
    //!
    //! @param p     The position at which new elements will be inserted.
    //! @param count The number of new elements which will be inserted.
    //! @param value The value used to copy construct new elements.
    //!
    //! @par Throws
    //!   @li If T's copy constructor or copy assignment throws.
    //!   @li If T's move constructor or move assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    iterator insert(const_iterator p, size_type count, value_type const& value);

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>.
    //!  @li <tt>distance(first, last) <= capacity()</tt>
    //!  @li \c Iterator must meet the \c ForwardTraversalIterator concept.
    //!
    //! @brief Inserts a copy of a range <tt>[first, last)</tt> at p.
    //!
    //! @param p     The position at which new elements will be inserted.
    //! @param first The iterator to the first element of a range used to construct new elements.
    //! @param last  The iterator to the one after the last element of a range used to construct new elements.
    //!
    //! @par Throws
    //!   @li If T's constructor and assignment taking a dereferenced \c Iterator.
    //!   @li If T's move constructor or move assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <typename Iterator>
    iterator insert(const_iterator p, Iterator first, Iterator last);

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>.
    //!  @li <tt>distance(il.begin(), il.end()) <= capacity()</tt>
    //!
    //! @brief Inserts a copy of a range <tt>[il.begin(), il.end())</tt> at p.
    //!
    //! @param p     The position at which new elements will be inserted.
    //! @param il    The std::initializer_list which contains elements that will be inserted.
    //!
    //! @par Throws
    //!   @li If T's constructor and assignment taking a dereferenced std::initializer_list iterator.
    //!
    //! @par Complexity
    //!   Linear O(N).
    iterator insert(const_iterator p, std::initializer_list<value_type> il);

    //! @pre \c p must be a valid iterator of \c *this in range <tt>[begin(), end())</tt>
    //!
    //! @brief Erases T from p.
    //!
    //! @param p    The position of the element which will be erased from the container.
    //!
    //! @par Throws
    //!   If T's move assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    iterator erase(const_iterator p);

    //! @pre
    //!  @li \c first and \c last must define a valid range
    //!  @li iterators must be in range <tt>[begin(), end()]</tt>
    //!
    //! @brief Erases Values from a range <tt>[first, last)</tt>.
    //!
    //! @param first    The position of the first element of a range which will be erased from the container.
    //! @param last     The position of the one after the last element of a range which will be erased from the container.
    //!
    //! @par Throws
    //!   If T's move assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    iterator erase(const_iterator first, const_iterator last);

    //! @pre <tt>distance(first, last) <= capacity()</tt>
    //!
    //! @brief Assigns a range <tt>[first, last)</tt> of Values to this container.
    //!
    //! @param first       The iterator to the first element of a range used to construct new content of this container.
    //! @param last        The iterator to the one after the last element of a range used to construct new content of this container.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws,
    //!
    //! @par Complexity
    //!   Linear O(N).
    template <typename Iterator>
    void assign(Iterator first, Iterator last);

    //! @pre <tt>distance(il.begin(), il.end()) <= capacity()</tt>
    //!
    //! @brief Assigns a range <tt>[il.begin(), il.end())</tt> of Values to this container.
    //!
    //! @param il       std::initializer_list with values used to construct new content of this container.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws,
    //!
    //! @par Complexity
    //!   Linear O(N).
    void assign(std::initializer_list<value_type> il);

    //! @pre <tt>count <= capacity()</tt>
    //!
    //! @brief Assigns a count copies of value to this container.
    //!
    //! @param count       The new number of elements which will be container in the container.
    //! @param value       The value which will be used to copy construct the new content.
    //!
    //! @par Throws
    //!   If T's copy constructor or copy assignment throws.
    //!
    //! @par Complexity
    //!   Linear O(N).
    void assign(size_type count, value_type const& value);

    //! @pre <tt>size() < capacity()</tt>
    //!
    //! @brief Inserts a T constructed with
    //!   \c std::forward<Args>(args)... in the end of the container.
    //!
    //! @return A reference to the created object.
    //!
    //! @param args     The arguments of the constructor of the new element which will be created at the end of the container.
    //!
    //! @par Throws
    //!   If in-place constructor throws or T's move constructor throws.
    //!
    //! @par Complexity
    //!   Constant O(1).
    template<class ...Args>
    reference emplace_back(Args &&...args);

    //! @pre
    //!  @li \c p must be a valid iterator of \c *this in range <tt>[begin(), end()]</tt>
    //!  @li <tt>size() < capacity()</tt>
    //!
    //! @brief Inserts a T constructed with
    //!   \c std::forward<Args>(args)... before p
    //!
    //! @param p     The position at which new elements will be inserted.
    //! @param args  The arguments of the constructor of the new element.
    //!
    //! @par Throws
    //!   If in-place constructor throws or if T's move constructor or move assignment throws.
    //!
    //! @par Complexity
    //!   Constant or linear.
    template<class ...Args>
    iterator emplace(const_iterator p, Args &&...args);

    //! @brief Removes all elements from the container.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    void clear()  BOOST_NOEXCEPT_OR_NOTHROW;

    //! @pre <tt>i < size()</tt>
    //!
    //! @brief Returns reference to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return reference to the i-th element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   \c std::out_of_range exception by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reference at(size_type i);

    //! @pre <tt>i < size()</tt>
    //!
    //! @brief Returns const reference to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return const reference to the i-th element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   \c std::out_of_range exception by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reference at(size_type i) const;

    //! @pre <tt>i < size()</tt>
    //!
    //! @brief Returns reference to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return reference to the i-th element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reference operator[](size_type i);

    //! @pre <tt>i < size()</tt>
    //!
    //! @brief Returns const reference to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return const reference to the i-th element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reference operator[](size_type i) const;

    //! @pre <tt>i =< size()</tt>
    //!
    //! @brief Returns a iterator to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return a iterator to the i-th element.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    iterator nth(size_type i);

    //! @pre <tt>i =< size()</tt>
    //!
    //! @brief Returns a const_iterator to the i-th element.
    //!
    //! @param i    The element's index.
    //!
    //! @return a const_iterator to the i-th element.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_iterator nth(size_type i) const;

    //! @pre <tt>begin() <= p <= end()</tt>
    //!
    //! @brief Returns the index of the element pointed by p.
    //!
    //! @param p    An iterator to the element.
    //!
    //! @return The index of the element pointed by p.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    size_type index_of(iterator p);

    //! @pre <tt>begin() <= p <= end()</tt>
    //!
    //! @brief Returns the index of the element pointed by p.
    //!
    //! @param p    A const_iterator to the element.
    //!
    //! @return a const_iterator to the i-th element.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    size_type index_of(const_iterator p) const;

    //! @pre \c !empty()
    //!
    //! @brief Returns reference to the first element.
    //!
    //! @return reference to the first element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reference front();

    //! @pre \c !empty()
    //!
    //! @brief Returns const reference to the first element.
    //!
    //! @return const reference to the first element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reference front() const;

    //! @pre \c !empty()
    //!
    //! @brief Returns reference to the last element.
    //!
    //! @return reference to the last element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reference back();

    //! @pre \c !empty()
    //!
    //! @brief Returns const reference to the first element.
    //!
    //! @return const reference to the last element
    //!   from the beginning of the container.
    //!
    //! @par Throws
    //!   Nothing by default.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reference back() const;

    //! @brief Pointer such that <tt>[data(), data() + size())</tt> is a valid range.
    //!   For a non-empty vector <tt>data() == &front()</tt>.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    T * data() BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Const pointer such that <tt>[data(), data() + size())</tt> is a valid range.
    //!   For a non-empty vector <tt>data() == &front()</tt>.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const T * data() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns iterator to the first element.
    //!
    //! @return iterator to the first element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    iterator begin() BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const iterator to the first element.
    //!
    //! @return const_iterator to the first element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_iterator begin() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const iterator to the first element.
    //!
    //! @return const_iterator to the first element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_iterator cbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns iterator to the one after the last element.
    //!
    //! @return iterator pointing to the one after the last element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    iterator end() BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const iterator to the one after the last element.
    //!
    //! @return const_iterator pointing to the one after the last element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_iterator end() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const iterator to the one after the last element.
    //!
    //! @return const_iterator pointing to the one after the last element contained in the vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_iterator cend() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns reverse iterator to the first element of the reversed container.
    //!
    //! @return reverse_iterator pointing to the beginning
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reverse_iterator rbegin() BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const reverse iterator to the first element of the reversed container.
    //!
    //! @return const_reverse_iterator pointing to the beginning
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reverse_iterator rbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const reverse iterator to the first element of the reversed container.
    //!
    //! @return const_reverse_iterator pointing to the beginning
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reverse_iterator crbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns reverse iterator to the one after the last element of the reversed container.
    //!
    //! @return reverse_iterator pointing to the one after the last element
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    reverse_iterator rend() BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const reverse iterator to the one after the last element of the reversed container.
    //!
    //! @return const_reverse_iterator pointing to the one after the last element
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reverse_iterator rend() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Returns const reverse iterator to the one after the last element of the reversed container.
    //!
    //! @return const_reverse_iterator pointing to the one after the last element
    //! of the reversed static_vector.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    const_reverse_iterator crend() const BOOST_NOEXCEPT_OR_NOTHROW;

   #endif   //#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

   //! @brief Returns container's capacity.
   //!
   //! @return container's capacity.
   //!
   //! @par Throws
   //!   Nothing.
   //!
   //! @par Complexity
   //!   Constant O(1).
   BOOST_CONTAINER_FORCEINLINE static size_type capacity() BOOST_NOEXCEPT_OR_NOTHROW
   { return static_capacity; }

   //! @brief Returns container's capacity.
   //!
   //! @return container's capacity.
   //!
   //! @par Throws
   //!   Nothing.
   //!
   //! @par Complexity
   //!   Constant O(1).
   BOOST_CONTAINER_FORCEINLINE static size_type max_size() BOOST_NOEXCEPT_OR_NOTHROW
   { return static_capacity; }

   #ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

    //! @brief Returns the number of stored elements.
    //!
    //! @return Number of elements contained in the container.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    size_type size() const BOOST_NOEXCEPT_OR_NOTHROW;

    //! @brief Queries if the container contains elements.
    //!
    //! @return true if the number of elements contained in the
    //!   container is equal to 0.
    //!
    //! @par Throws
    //!   Nothing.
    //!
    //! @par Complexity
    //!   Constant O(1).
    bool empty() const BOOST_NOEXCEPT_OR_NOTHROW;
#else

   BOOST_CONTAINER_FORCEINLINE friend void swap(static_vector &x, static_vector &y)
   {
      x.swap(y);
   }

#endif // BOOST_CONTAINER_DOXYGEN_INVOKED

};

#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

//! @brief Checks if contents of two static_vectors are equal.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if containers have the same size and elements in both containers are equal.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator== (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Checks if contents of two static_vectors are not equal.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if containers have different size or elements in both containers are not equal.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator!= (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Lexicographically compares static_vectors.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if x compares lexicographically less than y.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator< (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Lexicographically compares static_vectors.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if y compares lexicographically less than x.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator> (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Lexicographically compares static_vectors.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if y don't compare lexicographically less than x.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator<= (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Lexicographically compares static_vectors.
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @return     \c true if x don't compare lexicographically less than y.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
bool operator>= (static_vector<V, C1, O1> const& x, static_vector<V, C2, O2> const& y);

//! @brief Swaps contents of two static_vectors.
//!
//! This function calls static_vector::swap().
//!
//! @ingroup static_vector_non_member
//!
//! @param x    The first static_vector.
//! @param y    The second static_vector.
//!
//! @par Complexity
//!   Linear O(N).
template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
inline void swap(static_vector<V, C1, O1> & x, static_vector<V, C2, O2> & y);

#else

template<typename V, std::size_t C1, std::size_t C2, class O1, class O2>
inline void swap(static_vector<V, C1, O1> & x, static_vector<V, C2, O2> & y
      , typename dtl::enable_if_c< C1 != C2>::type * = 0)
{
   x.swap(y);
}

#endif // BOOST_CONTAINER_DOXYGEN_INVOKED

}} // namespace boost::container

#include <boost/container/detail/config_end.hpp>

#endif // BOOST_CONTAINER_STATIC_VECTOR_HPP
