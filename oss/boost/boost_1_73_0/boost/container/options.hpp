/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2013-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_OPTIONS_HPP
#define BOOST_CONTAINER_OPTIONS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/intrusive/pack_options.hpp>
#include <boost/static_assert.hpp>

namespace boost {
namespace container {

////////////////////////////////////////////////////////////////
//
//
//       OPTIONS FOR ASSOCIATIVE TREE-BASED CONTAINERS
//
//
////////////////////////////////////////////////////////////////

//! Enumeration used to configure ordered associative containers
//! with a concrete tree implementation.
enum tree_type_enum
{
   red_black_tree,
   avl_tree,
   scapegoat_tree,
   splay_tree
};

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<tree_type_enum TreeType, bool OptimizeSize>
struct tree_opt
{
   static const boost::container::tree_type_enum tree_type = TreeType;
   static const bool optimize_size = OptimizeSize;
};

typedef tree_opt<red_black_tree, true> tree_assoc_defaults;

#endif   //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//!This option setter specifies the underlying tree type
//!(red-black, AVL, Scapegoat or Splay) for ordered associative containers
BOOST_INTRUSIVE_OPTION_CONSTANT(tree_type, tree_type_enum, TreeType, tree_type)

//!This option setter specifies if node size is optimized
//!storing rebalancing data masked into pointers for ordered associative containers
BOOST_INTRUSIVE_OPTION_CONSTANT(optimize_size, bool, Enabled, optimize_size)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::set, \c boost::container::multiset
//! \c boost::container::map and \c boost::container::multimap.
//! Supported options are: \c boost::container::optimize_size and \c boost::container::tree_type
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct tree_assoc_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < tree_assoc_defaults,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef tree_opt<packed_options::tree_type, packed_options::optimize_size> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by tree-based associative containers
template<class ...Options>
using tree_assoc_options_t = typename boost::container::tree_assoc_options<Options...>::type;

#endif


////////////////////////////////////////////////////////////////
//
//
//       OPTIONS FOR ASSOCIATIVE HASH-BASED CONTAINERS
//
//
////////////////////////////////////////////////////////////////

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<bool StoreHash>
struct hash_opt
{
   static const bool store_hash = StoreHash;
};

typedef hash_opt<false> hash_assoc_defaults;

#endif   //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//!This option setter specifies if node size is optimized
//!storing rebalancing data masked into pointers for ordered associative containers
BOOST_INTRUSIVE_OPTION_CONSTANT(store_hash, bool, Enabled, store_hash)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::hash_set, \c boost::container::hash_multiset
//! \c boost::container::hash_map and \c boost::container::hash_multimap.
//! Supported options are: \c boost::container::store_hash
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct hash_assoc_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < hash_assoc_defaults,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef hash_opt<packed_options::store_hash> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by hash-based associative containers
template<class ...Options>
using hash_assoc_options_t = typename boost::container::hash_assoc_options<Options...>::type;

#endif

////////////////////////////////////////////////////////////////
//
//
//          OPTIONS FOR VECTOR-BASED CONTAINERS
//
//
////////////////////////////////////////////////////////////////

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<class T, class Default>
struct default_if_void
{
   typedef T type;
};

template<class Default>
struct default_if_void<void, Default>
{
   typedef Default type;
};

#endif

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<class AllocTraits, class StoredSizeType>
struct get_stored_size_type_with_alloctraits
{
   typedef StoredSizeType type;
};

template<class AllocTraits>
struct get_stored_size_type_with_alloctraits<AllocTraits, void>
{
   typedef typename AllocTraits::size_type type;
};

template<class GrowthType, class StoredSizeType>
struct vector_opt
{
   typedef GrowthType      growth_factor_type;
   typedef StoredSizeType  stored_size_type;

   template<class AllocTraits>
   struct get_stored_size_type
      : get_stored_size_type_with_alloctraits<AllocTraits, StoredSizeType>
   {};
};

class default_next_capacity;

typedef vector_opt<void, void> vector_null_opt;

#else    //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//!This growth factor argument specifies that the container should increase it's
//!capacity a 50% when existing capacity is exhausted.
struct growth_factor_50{};

//!This growth factor argument specifies that the container should increase it's
//!capacity a 60% when existing capacity is exhausted.
struct growth_factor_60{};

//!This growth factor argument specifies that the container should increase it's
//!capacity a 100% (doubling its capacity) when existing capacity is exhausted.
struct growth_factor_100{};

#endif   //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//!This option setter specifies the growth factor strategy of the underlying vector.
//!
//!\tparam GrowthFactor A function object that has the following signature:<br/><br/>
//!`template<class SizeType>`<br/>
//!`SizeType operator()(SizeType cur_cap, SizeType add_min_cap, SizeType max_cap) const;`.<br/><br/>
//!`cur_cap` is the current capacity, `add_min_cap` is the minimum additional capacity
//!we want to achieve and `max_cap` is the maximum capacity that the allocator or other 
//!factors allow. The implementation should return a value between `cur_cap` + `add_min_cap`
//!and `max_cap`. `cur_cap` + `add_min_cap` is guaranteed not to overflow/wraparound,
//! but the implementation should handle wraparound produced by the growth factor.
//!
//!Predefined growth factors that can be passed as arguments to this option are:
//!\c boost::container::growth_factor_50
//!\c boost::container::growth_factor_60
//!\c boost::container::growth_factor_100
//!
//!If this option is not specified, a default will be used by the container.
BOOST_INTRUSIVE_OPTION_TYPE(growth_factor, GrowthFactor, GrowthFactor, growth_factor_type)

//!This option specifies the unsigned integer type that a user wants the container
//!to use to hold size-related information inside a container (e.g. current size, current capacity).
//!
//!\tparam StoredSizeType An unsigned integer type. It shall be smaller than than the size
//! of the size_type deduced from `allocator_traits<A>::size_type` or the same type.
//!
//!If the maximum capacity() to be used is limited, a user can try to use 8-bit, 16-bit 
//!(e.g. in 32-bit machines), or 32-bit size types (e.g. in a 64 bit machine) to see if some
//!memory can be saved for empty vectors. This could potentially performance benefits due to better
//!cache usage.
//!
//!Note that alignment requirements can disallow theoretical space savings. Example:
//!\c vector holds a pointer and two size types (for size and capacity), in a 32 bit machine
//!a 8 bit size type (total size: 4 byte pointer + 2 x 1 byte sizes = 6 bytes) 
//!will not save space when comparing two 16-bit size types because usually
//!a 32 bit alignment is required for vector and the size will be rounded to 8 bytes. In a 64-bit
//!machine a 16 bit size type does not usually save memory when comparing to a 32-bit size type.
//!Measure the size of the resulting container and do not assume a smaller \c stored_size
//!will always lead to a smaller sizeof(container).
//!
//!If a user tries to insert more elements than representable by \c stored_size, vector
//!will throw a length_error.
//!
//!If this option is not specified, `allocator_traits<A>::size_type` (usually std::size_t) will
//!be used to store size-related information inside the container.
BOOST_INTRUSIVE_OPTION_TYPE(stored_size, StoredSizeType, StoredSizeType, stored_size_type)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::vector.
//! Supported options are: \c boost::container::growth_factor and \c boost::container::stored_size
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct vector_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < vector_null_opt,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef vector_opt< typename packed_options::growth_factor_type
                     , typename packed_options::stored_size_type> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by \c boost::container::vector.
//! Supported options are: \c boost::container::growth_factor and \c boost::container::stored_size
template<class ...Options>
using vector_options_t = typename boost::container::vector_options<Options...>::type;

#endif

////////////////////////////////////////////////////////////////
//
//
//          OPTIONS FOR SMALL-VECTOR CONTAINER
//
//
////////////////////////////////////////////////////////////////

//! This option specifies the desired alignment for the value_type stored
//! in the container.
//! A value zero represents the natural alignment.
//!
//!\tparam Alignment An unsigned integer value. Must be power of two.
BOOST_INTRUSIVE_OPTION_CONSTANT(inplace_alignment, std::size_t, Alignment, inplace_alignment)

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<class GrowthType, std::size_t InplaceAlignment>
struct small_vector_opt
{
   typedef GrowthType      growth_factor_type;
   static const std::size_t inplace_alignment = InplaceAlignment;
};

typedef small_vector_opt<void, 0u> small_vector_null_opt;

#endif    //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::small_vector.
//! Supported options are: \c boost::container::growth_factor and \c boost::container::inplace_alignment
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct small_vector_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < small_vector_null_opt,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef small_vector_opt< typename packed_options::growth_factor_type
                           , packed_options::inplace_alignment> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by \c boost::container::small_vector.
//! Supported options are: \c boost::container::growth_factor and \c boost::container::stored_size
template<class ...Options>
using small_vector_options_t = typename boost::container::small_vector_options<Options...>::type;

#endif


////////////////////////////////////////////////////////////////
//
//
//          OPTIONS FOR STATIC-VECTOR CONTAINER
//
//
////////////////////////////////////////////////////////////////

//!This option specifies if the container will throw if in
//!the static capacity is not sufficient to hold the required
//!values. If false is specified, insufficient capacity will
//!lead to BOOST_ASSERT, and if this assertion returns, to undefined behaviour,
//!which potentially can lead to better static_vector performance.
//!The default value is true.
//!
//!\tparam ThrowOnExhaustion A boolean value. True if throw is required.
BOOST_INTRUSIVE_OPTION_CONSTANT(throw_on_overflow, bool, ThrowOnOverflow, throw_on_overflow)

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<bool ThrowOnOverflow, std::size_t InplaceAlignment>
struct static_vector_opt
{
   static const bool throw_on_overflow = ThrowOnOverflow;
   static const std::size_t inplace_alignment = InplaceAlignment;
};

typedef static_vector_opt<true, 0u> static_vector_null_opt;

#endif    //!defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::static_vector.
//! Supported options are: \c boost::container::throw_on_overflow and \c boost::container::inplace_alignment
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct static_vector_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < static_vector_null_opt,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef static_vector_opt< packed_options::throw_on_overflow
                            , packed_options::inplace_alignment> implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by \c boost::container::static_vector.
//! Supported options are: \c boost::container::growth_factor and \c boost::container::stored_size
template<class ...Options>
using static_vector_options_t = typename boost::container::static_vector_options<Options...>::type;

#endif


////////////////////////////////////////////////////////////////
//
//
//          OPTIONS FOR DEQUE-BASED CONTAINERS
//
//
////////////////////////////////////////////////////////////////

#if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)

template<std::size_t BlockBytes, std::size_t BlockSize>
struct deque_opt
{
   static const std::size_t block_bytes = BlockBytes;
   static const std::size_t block_size  = BlockSize;
   BOOST_STATIC_ASSERT_MSG(!(block_bytes && block_size), "block_bytes and block_size can't be specified at the same time");
};

typedef deque_opt<0u, 0u> deque_null_opt;

#endif

//! Helper metafunction to combine options into a single type to be used
//! by \c boost::container::deque.
//! Supported options are: \c boost::container::block_bytes
#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
template<class ...Options>
#else
template<class O1 = void, class O2 = void, class O3 = void, class O4 = void>
#endif
struct deque_options
{
   /// @cond
   typedef typename ::boost::intrusive::pack_options
      < deque_null_opt,
      #if !defined(BOOST_CONTAINER_VARIADIC_TEMPLATES)
      O1, O2, O3, O4
      #else
      Options...
      #endif
      >::type packed_options;
   typedef deque_opt< packed_options::block_bytes, packed_options::block_size > implementation_defined;
   /// @endcond
   typedef implementation_defined type;
};

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

//! Helper alias metafunction to combine options into a single type to be used
//! by \c boost::container::deque.
//! Supported options are: \c boost::container::block_bytes
template<class ...Options>
using deque_options_t = typename boost::container::deque_options<Options...>::type;

#endif

//!This option specifies the maximum size of a block in bytes: this delimites the number of contiguous elements
//!that will be allocated by deque as min(1u, BlockBytes/sizeof(value_type))
//!A value zero represents the default value.
//!
//!\tparam BlockBytes An unsigned integer value.
BOOST_INTRUSIVE_OPTION_CONSTANT(block_bytes, std::size_t, BlockBytes, block_bytes)

//!This option specifies the size of a block, delimites the number of contiguous elements
//!that will be allocated by deque as BlockSize.
//!A value zero represents the default value.
//!
//!\tparam BlockBytes An unsigned integer value.
BOOST_INTRUSIVE_OPTION_CONSTANT(block_size, std::size_t, BlockSize, block_size)

}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_OPTIONS_HPP
