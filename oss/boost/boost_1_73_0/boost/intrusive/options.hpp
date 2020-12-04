/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2007-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_OPTIONS_HPP
#define BOOST_INTRUSIVE_OPTIONS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/pack_options.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

struct empty
{};

template<class Functor>
struct fhtraits;

template<class T, class Hook, Hook T::* P>
struct mhtraits;

struct dft_tag;
struct member_tag;

template<class SupposedValueTraits>
struct is_default_hook_tag;

#endif   //#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

//!This option setter specifies if the intrusive
//!container stores its size as a member to
//!obtain constant-time size() member.
BOOST_INTRUSIVE_OPTION_CONSTANT(constant_time_size, bool, Enabled, constant_time_size)

//!This option setter specifies a container header holder type
BOOST_INTRUSIVE_OPTION_TYPE(header_holder_type, HeaderHolder, HeaderHolder, header_holder_type)

//!This option setter specifies the type that
//!the container will use to store its size.
BOOST_INTRUSIVE_OPTION_TYPE(size_type, SizeType, SizeType, size_type)

//!This option setter specifies the strict weak ordering
//!comparison functor for the value type
BOOST_INTRUSIVE_OPTION_TYPE(compare, Compare, Compare, compare)

//!This option setter specifies a function object
//!that specifies the type of the key of an associative
//!container and an operator to obtain it from a value type.
//!
//!This function object must the define a `type` member typedef and
//!a member with signature `type [const&] operator()(const value_type &) const`
//!that will return the key from a value_type of an associative container
BOOST_INTRUSIVE_OPTION_TYPE(key_of_value, KeyOfValue, KeyOfValue, key_of_value)

//!This option setter specifies a function object
//!that specifies the type of the priority of a treap
//!container and an operator to obtain it from a value type.
//!
//!This function object must the define a `type` member typedef and
//!a member with signature `type [const&] operator()(const value_type &) const`
//!that will return the priority from a value_type of a treap container
BOOST_INTRUSIVE_OPTION_TYPE(priority_of_value, PrioOfValue, PrioOfValue, priority_of_value)

//!This option setter for scapegoat containers specifies if
//!the intrusive scapegoat container should use a non-variable
//!alpha value that does not need floating-point operations.
//!
//!If activated, the fixed alpha value is 1/sqrt(2). This
//!option also saves some space in the container since
//!the alpha value and some additional data does not need
//!to be stored in the container.
//!
//!If the user only needs an alpha value near 1/sqrt(2), this
//!option also improves performance since avoids logarithm
//!and division operations when rebalancing the tree.
BOOST_INTRUSIVE_OPTION_CONSTANT(floating_point, bool, Enabled, floating_point)

//!This option setter specifies the equality
//!functor for the value type
BOOST_INTRUSIVE_OPTION_TYPE(equal, Equal, Equal, equal)

//!This option setter specifies the priority comparison
//!functor for the value type
BOOST_INTRUSIVE_OPTION_TYPE(priority, Priority, Priority, priority)

//!This option setter specifies the hash
//!functor for the value type
BOOST_INTRUSIVE_OPTION_TYPE(hash, Hash, Hash, hash)

//!This option setter specifies the relationship between the type
//!to be managed by the container (the value type) and the node to be
//!used in the node algorithms. It also specifies the linking policy.
BOOST_INTRUSIVE_OPTION_TYPE(value_traits, ValueTraits, ValueTraits, proto_value_traits)

//#define BOOST_INTRUSIVE_COMMA ,
//#define BOOST_INTRUSIVE_LESS <
//#define BOOST_INTRUSIVE_MORE >
//BOOST_INTRUSIVE_OPTION_TYPE (member_hook, Parent BOOST_INTRUSIVE_COMMA class MemberHook BOOST_INTRUSIVE_COMMA MemberHook Parent::* PtrToMember , mhtraits BOOST_INTRUSIVE_LESS Parent BOOST_INTRUSIVE_COMMA MemberHook BOOST_INTRUSIVE_COMMA PtrToMember BOOST_INTRUSIVE_MORE , proto_value_traits)
//template< class Parent , class MemberHook , MemberHook Parent::* PtrToMember>
//struct member_hook {
//   template<class Base> struct pack : Base {
//      typedef mhtraits < Parent , MemberHook , PtrToMember > proto_value_traits;
//   };
//};
//
//#undef BOOST_INTRUSIVE_COMMA
//#undef BOOST_INTRUSIVE_LESS
//#undef BOOST_INTRUSIVE_MORE

//!This option setter specifies the member hook the
//!container must use.
template< typename Parent
        , typename MemberHook
        , MemberHook Parent::* PtrToMember>
struct member_hook
{
// @cond
//   typedef typename MemberHook::hooktags::node_traits node_traits;
//   typedef typename node_traits::node node_type;
//   typedef node_type Parent::* Ptr2MemNode;
//   typedef mhtraits
//      < Parent
//      , node_traits
//      //This cast is really ugly but necessary to reduce template bloat.
//      //Since we control the layout between the hook and the node, and there is
//      //always single inheritance, the offset of the node is exactly the offset of
//      //the hook. Since the node type is shared between all member hooks, this saves
//      //quite a lot of symbol stuff.
//      , (Ptr2MemNode)PtrToMember
//      , MemberHook::hooktags::link_mode> member_value_traits;
   typedef mhtraits <Parent, MemberHook, PtrToMember> member_value_traits;
   template<class Base>
   struct pack : Base
   {
      typedef member_value_traits proto_value_traits;
   };
/// @endcond
};

//!This option setter specifies the function object that will
//!be used to convert between values to be inserted in a container
//!and the hook to be used for that purpose.
BOOST_INTRUSIVE_OPTION_TYPE(function_hook, Functor, fhtraits<Functor>, proto_value_traits)

//!This option setter specifies that the container
//!must use the specified base hook
BOOST_INTRUSIVE_OPTION_TYPE(base_hook, BaseHook, BaseHook, proto_value_traits)

//!This option setter specifies the type of
//!a void pointer. This will instruct the hook
//!to use this type of pointer instead of the
//!default one
BOOST_INTRUSIVE_OPTION_TYPE(void_pointer, VoidPointer, VoidPointer, void_pointer)

//!This option setter specifies the type of
//!the tag of a base hook. A type cannot have two
//!base hooks of the same type, so a tag can be used
//!to differentiate two base hooks with otherwise same type
BOOST_INTRUSIVE_OPTION_TYPE(tag, Tag, Tag, tag)

//!This option setter specifies the link mode
//!(normal_link, safe_link or auto_unlink)
BOOST_INTRUSIVE_OPTION_CONSTANT(link_mode, link_mode_type, LinkType, link_mode)

//!This option setter specifies if the hook
//!should be optimized for size instead of for speed.
BOOST_INTRUSIVE_OPTION_CONSTANT(optimize_size, bool, Enabled, optimize_size)

//!This option setter specifies if the slist container should
//!use a linear implementation instead of a circular one.
BOOST_INTRUSIVE_OPTION_CONSTANT(linear, bool, Enabled, linear)

//!If true, slist also stores a pointer to the last element of the singly linked list.
//!This allows O(1) swap and splice_after(iterator, slist &) for circular slists and makes
//!possible new functions like push_back(reference) and back().
BOOST_INTRUSIVE_OPTION_CONSTANT(cache_last, bool, Enabled, cache_last)

//!This option setter specifies the bucket traits
//!class for unordered associative containers. When this option is specified,
//!instead of using the default bucket traits, a user defined holder will be defined
BOOST_INTRUSIVE_OPTION_TYPE(bucket_traits, BucketTraits, BucketTraits, bucket_traits)

//!This option setter specifies if the unordered hook
//!should offer room to store the hash value.
//!Storing the hash in the hook will speed up rehashing
//!processes in applications where rehashing is frequent,
//!rehashing might throw or the value is heavy to hash.
BOOST_INTRUSIVE_OPTION_CONSTANT(store_hash, bool, Enabled, store_hash)

//!This option setter specifies if the unordered hook
//!should offer room to store another link to another node
//!with the same key.
//!Storing this link will speed up lookups and insertions on
//!unordered_multiset containers with a great number of elements
//!with the same key.
BOOST_INTRUSIVE_OPTION_CONSTANT(optimize_multikey, bool, Enabled, optimize_multikey)

//!This option setter specifies if the bucket array will be always power of two.
//!This allows using masks instead of the default modulo operation to determine
//!the bucket number from the hash value, leading to better performance.
//!In debug mode, if power of two buckets mode is activated, the bucket length
//!will be checked with assertions.
BOOST_INTRUSIVE_OPTION_CONSTANT(power_2_buckets, bool, Enabled, power_2_buckets)

//!This option setter specifies if the container will cache a pointer to the first
//!non-empty bucket so that begin() is always constant-time.
//!This is specially helpful when we can have containers with a few elements
//!but with big bucket arrays (that is, hashtables with low load factors).
BOOST_INTRUSIVE_OPTION_CONSTANT(cache_begin, bool, Enabled, cache_begin)

//!This option setter specifies if the container will compare the hash value
//!before comparing objects. This option can't be specified if store_hash<>
//!is not true.
//!This is specially helpful when we have containers with a high load factor.
//!and the comparison function is much more expensive that comparing already
//!stored hash values.
BOOST_INTRUSIVE_OPTION_CONSTANT(compare_hash, bool, Enabled, compare_hash)

//!This option setter specifies if the hash container will use incremental
//!hashing. With incremental hashing the cost of hash table expansion is spread
//!out across each hash table insertion operation, as opposed to be incurred all at once.
//!Therefore linear hashing is well suited for interactive applications or real-time
//!appplications where the worst-case insertion time of non-incremental hash containers
//!(rehashing the whole bucket array) is not admisible.
BOOST_INTRUSIVE_OPTION_CONSTANT(incremental, bool, Enabled, incremental)

/// @cond

struct hook_defaults
{
   typedef void* void_pointer;
   static const link_mode_type link_mode = safe_link;
   typedef dft_tag tag;
   static const bool optimize_size = false;
   static const bool store_hash = false;
   static const bool linear = false;
   static const bool optimize_multikey = false;
};

/// @endcond

}  //namespace intrusive {
}  //namespace boost {

#include <boost/intrusive/detail/config_end.hpp>

#endif   //#ifndef BOOST_INTRUSIVE_OPTIONS_HPP
