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

#ifndef BOOST_INTRUSIVE_CIRCULAR_SLIST_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_CIRCULAR_SLIST_ALGORITHMS_HPP

#include <cstddef>
#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/common_slist_algorithms.hpp>
#include <boost/intrusive/detail/algo_type.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

//! circular_slist_algorithms provides basic algorithms to manipulate nodes
//! forming a circular singly linked list. An empty circular list is formed by a node
//! whose pointer to the next node points to itself.
//!
//! circular_slist_algorithms is configured with a NodeTraits class, which encapsulates the
//! information about the node to be manipulated. NodeTraits must support the
//! following interface:
//!
//! <b>Typedefs</b>:
//!
//! <tt>node</tt>: The type of the node that forms the circular list
//!
//! <tt>node_ptr</tt>: A pointer to a node
//!
//! <tt>const_node_ptr</tt>: A pointer to a const node
//!
//! <b>Static functions</b>:
//!
//! <tt>static node_ptr get_next(const_node_ptr n);</tt>
//!
//! <tt>static void set_next(node_ptr n, node_ptr next);</tt>
template<class NodeTraits>
class circular_slist_algorithms
   /// @cond
   : public detail::common_slist_algorithms<NodeTraits>
   /// @endcond
{
   /// @cond
   typedef detail::common_slist_algorithms<NodeTraits> base_t;
   /// @endcond
   public:
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef NodeTraits                           node_traits;

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

   //! <b>Effects</b>: Constructs an non-used list element, putting the next
   //!   pointer to null:
   //!  <tt>NodeTraits::get_next(this_node) == node_ptr()</tt>
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void init(node_ptr this_node);

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns true is "this_node" is the only node of a circular list:
   //!  or it's a not inserted node:
   //!  <tt>return node_ptr() == NodeTraits::get_next(this_node) || NodeTraits::get_next(this_node) == this_node</tt>
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static bool unique(const_node_ptr this_node);

   //! <b>Effects</b>: Returns true is "this_node" has the same state as
   //!  if it was inited using "init(node_ptr)"
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static bool inited(const_node_ptr this_node);

   //! <b>Requires</b>: prev_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Unlinks the next node of prev_node from the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void unlink_after(node_ptr prev_node);

   //! <b>Requires</b>: prev_node and last_node must be in a circular list
   //!  or be an empty circular list.
   //!
   //! <b>Effects</b>: Unlinks the range (prev_node, last_node) from the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void unlink_after(node_ptr prev_node, node_ptr last_node);

   //! <b>Requires</b>: prev_node must be a node of a circular list.
   //!
   //! <b>Effects</b>: Links this_node after prev_node in the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void link_after(node_ptr prev_node, node_ptr this_node);

   //! <b>Requires</b>: b and e must be nodes of the same circular list or an empty range.
   //!   and p must be a node of a different circular list.
   //!
   //! <b>Effects</b>: Removes the nodes from (b, e] range from their circular list and inserts
   //!   them after p in p's circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void transfer_after(node_ptr p, node_ptr b, node_ptr e);

   #endif   //#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

   //! <b>Effects</b>: Constructs an empty list, making this_node the only
   //!   node of the circular list:
   //!  <tt>NodeTraits::get_next(this_node) == this_node</tt>.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void init_header(node_ptr this_node)
   {  NodeTraits::set_next(this_node, this_node);  }

   //! <b>Requires</b>: this_node and prev_init_node must be in the same circular list.
   //!
   //! <b>Effects</b>: Returns the previous node of this_node in the circular list starting.
   //!   the search from prev_init_node. The first node checked for equality
   //!   is NodeTraits::get_next(prev_init_node).
   //!
   //! <b>Complexity</b>: Linear to the number of elements between prev_init_node and this_node.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_previous_node(const node_ptr &prev_init_node, const node_ptr &this_node)
   {  return base_t::get_previous_node(prev_init_node, this_node);   }

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns the previous node of this_node in the circular list.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the circular list.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_previous_node(const node_ptr & this_node)
   {  return base_t::get_previous_node(this_node, this_node); }

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns the previous node of the previous node of this_node in the circular list.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the circular list.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_previous_previous_node(const node_ptr & this_node)
   {  return get_previous_previous_node(this_node, this_node); }

   //! <b>Requires</b>: this_node and p must be in the same circular list.
   //!
   //! <b>Effects</b>: Returns the previous node of the previous node of this_node in the
   //!   circular list starting. the search from p. The first node checked
   //!   for equality is NodeTraits::get_next((NodeTraits::get_next(p)).
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the circular list.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr get_previous_previous_node(node_ptr p, const node_ptr & this_node)
   {
      node_ptr p_next = NodeTraits::get_next(p);
      node_ptr p_next_next = NodeTraits::get_next(p_next);
      while (this_node != p_next_next){
         p = p_next;
         p_next = p_next_next;
         p_next_next = NodeTraits::get_next(p_next);
      }
      return p;
   }

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns the number of nodes in a circular list. If the circular list
   //!  is empty, returns 1.
   //!
   //! <b>Complexity</b>: Linear
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t count(const const_node_ptr & this_node)
   {
      std::size_t result = 0;
      const_node_ptr p = this_node;
      do{
         p = NodeTraits::get_next(p);
         ++result;
      } while (p != this_node);
      return result;
   }

   //! <b>Requires</b>: this_node must be in a circular list, be an empty circular list or be inited.
   //!
   //! <b>Effects</b>: Unlinks the node from the circular list.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the circular list
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void unlink(node_ptr this_node)
   {
      if(NodeTraits::get_next(this_node))
         base_t::unlink_after(get_previous_node(this_node));
   }

   //! <b>Requires</b>: nxt_node must be a node of a circular list.
   //!
   //! <b>Effects</b>: Links this_node before nxt_node in the circular list.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the circular list.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void link_before (node_ptr nxt_node, node_ptr this_node)
   {  base_t::link_after(get_previous_node(nxt_node), this_node);   }

   //! <b>Requires</b>: this_node and other_node must be nodes inserted
   //!  in circular lists or be empty circular lists.
   //!
   //! <b>Effects</b>: Swaps the position of the nodes: this_node is inserted in
   //!   other_nodes position in the second circular list and the other_node is inserted
   //!   in this_node's position in the first circular list.
   //!
   //! <b>Complexity</b>: Linear to number of elements of both lists
   //!
   //! <b>Throws</b>: Nothing.
   static void swap_nodes(node_ptr this_node, node_ptr other_node)
   {
      if (other_node == this_node)
         return;
      const node_ptr this_next = NodeTraits::get_next(this_node);
      const node_ptr other_next = NodeTraits::get_next(other_node);
      const bool this_null   = !this_next;
      const bool other_null  = !other_next;
      const bool this_empty  = this_next == this_node;
      const bool other_empty = other_next == other_node;

      if(!(other_null || other_empty)){
         NodeTraits::set_next(this_next == other_node ? other_node : get_previous_node(other_node), this_node );
      }
      if(!(this_null | this_empty)){
         NodeTraits::set_next(other_next == this_node ? this_node  : get_previous_node(this_node), other_node );
      }
      NodeTraits::set_next(this_node,  other_empty ? this_node  : (other_next == this_node ? other_node : other_next) );
      NodeTraits::set_next(other_node, this_empty  ? other_node : (this_next == other_node ? this_node :  this_next ) );
   }

   //! <b>Effects</b>: Reverses the order of elements in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: This function is linear to the contained elements.
   static void reverse(node_ptr p)
   {
      node_ptr i = NodeTraits::get_next(p), e(p);
      for (;;) {
         node_ptr nxt(NodeTraits::get_next(i));
         if (nxt == e)
            break;
         base_t::transfer_after(e, i, nxt);
      }
   }

   //! <b>Effects</b>: Moves the node p n positions towards the end of the list.
   //!
   //! <b>Returns</b>: The previous node of p after the function if there has been any movement,
   //!   Null if n leads to no movement.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number moved positions.
   static node_ptr move_backwards(node_ptr p, std::size_t n)
   {
      //Null shift, nothing to do
      if(!n) return node_ptr();
      node_ptr first  = NodeTraits::get_next(p);

      //count() == 1 or 2, nothing to do
      if(NodeTraits::get_next(first) == p)
         return node_ptr();

      bool end_found = false;
      node_ptr new_last = node_ptr();

      //Now find the new last node according to the shift count.
      //If we find p before finding the new last node
      //unlink p, shortcut the search now that we know the size of the list
      //and continue.
      for(std::size_t i = 1; i <= n; ++i){
         new_last = first;
         first = NodeTraits::get_next(first);
         if(first == p){
            //Shortcut the shift with the modulo of the size of the list
            n %= i;
            if(!n)
               return node_ptr();
            i = 0;
            //Unlink p and continue the new first node search
            first = NodeTraits::get_next(p);
            base_t::unlink_after(new_last);
            end_found = true;
         }
      }

      //If the p has not been found in the previous loop, find it
      //starting in the new first node and unlink it
      if(!end_found){
         base_t::unlink_after(base_t::get_previous_node(first, p));
      }

      //Now link p after the new last node
      base_t::link_after(new_last, p);
      return new_last;
   }

   //! <b>Effects</b>: Moves the node p n positions towards the beginning of the list.
   //!
   //! <b>Returns</b>: The previous node of p after the function if there has been any movement,
   //!   Null if n leads equals to no movement.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number moved positions.
   static node_ptr move_forward(node_ptr p, std::size_t n)
   {
      //Null shift, nothing to do
      if(!n) return node_ptr();
      node_ptr first  = node_traits::get_next(p);

      //count() == 1 or 2, nothing to do
      if(node_traits::get_next(first) == p) return node_ptr();

      //Iterate until p is found to know where the current last node is.
      //If the shift count is less than the size of the list, we can also obtain
      //the position of the new last node after the shift.
      node_ptr old_last(first), next_to_it, new_last(p);
      std::size_t distance = 1;
      while(p != (next_to_it = node_traits::get_next(old_last))){
         if(++distance > n)
            new_last = node_traits::get_next(new_last);
         old_last = next_to_it;
      }
      //If the shift was bigger or equal than the size, obtain the equivalent
      //forward shifts and find the new last node.
      if(distance <= n){
         //Now find the equivalent forward shifts.
         //Shortcut the shift with the modulo of the size of the list
         std::size_t new_before_last_pos = (distance - (n % distance))% distance;
         //If the shift is a multiple of the size there is nothing to do
         if(!new_before_last_pos)   return node_ptr();

         for( new_last = p
            ; new_before_last_pos--
            ; new_last = node_traits::get_next(new_last)){
            //empty
         }
      }

      //Now unlink p and link it after the new last node
      base_t::unlink_after(old_last);
      base_t::link_after(new_last, p);
      return new_last;
   }
};

/// @cond

template<class NodeTraits>
struct get_algo<CircularSListAlgorithms, NodeTraits>
{
   typedef circular_slist_algorithms<NodeTraits> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_CIRCULAR_SLIST_ALGORITHMS_HPP
