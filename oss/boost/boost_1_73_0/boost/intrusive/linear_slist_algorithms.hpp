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

#ifndef BOOST_INTRUSIVE_LINEAR_SLIST_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_LINEAR_SLIST_ALGORITHMS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/common_slist_algorithms.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <cstddef>
#include <boost/intrusive/detail/minimal_pair_header.hpp>   //std::pair

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

//! linear_slist_algorithms provides basic algorithms to manipulate nodes
//! forming a linear singly linked list.
//!
//! linear_slist_algorithms is configured with a NodeTraits class, which encapsulates the
//! information about the node to be manipulated. NodeTraits must support the
//! following interface:
//!
//! <b>Typedefs</b>:
//!
//! <tt>node</tt>: The type of the node that forms the linear list
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
class linear_slist_algorithms
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
   static void init(const node_ptr & this_node);

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

   //! <b>Effects</b>: Returns true is "this_node" has the same state as if
   //!  it was inited using "init(node_ptr)"
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
   static void unlink_after(const node_ptr & prev_node);

   //! <b>Requires</b>: prev_node and last_node must be in a circular list
   //!  or be an empty circular list.
   //!
   //! <b>Effects</b>: Unlinks the range (prev_node, last_node) from the linear list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void unlink_after(const node_ptr & prev_node, const node_ptr & last_node);

   //! <b>Requires</b>: prev_node must be a node of a linear list.
   //!
   //! <b>Effects</b>: Links this_node after prev_node in the linear list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void link_after(const node_ptr & prev_node, const node_ptr & this_node);

   //! <b>Requires</b>: b and e must be nodes of the same linear list or an empty range.
   //!   and p must be a node of a different linear list.
   //!
   //! <b>Effects</b>: Removes the nodes from (b, e] range from their linear list and inserts
   //!   them after p in p's linear list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void transfer_after(const node_ptr & p, const node_ptr & b, const node_ptr & e);

   #endif   //#if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)

   //! <b>Effects</b>: Constructs an empty list, making this_node the only
   //!   node of the circular list:
   //!  <tt>NodeTraits::get_next(this_node) == this_node</tt>.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void init_header(const node_ptr & this_node)
   {  NodeTraits::set_next(this_node, node_ptr ());  }

   //! <b>Requires</b>: this_node and prev_init_node must be in the same linear list.
   //!
   //! <b>Effects</b>: Returns the previous node of this_node in the linear list starting.
   //!   the search from prev_init_node. The first node checked for equality
   //!   is NodeTraits::get_next(prev_init_node).
   //!
   //! <b>Complexity</b>: Linear to the number of elements between prev_init_node and this_node.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_previous_node(const node_ptr & prev_init_node, const node_ptr & this_node)
   {  return base_t::get_previous_node(prev_init_node, this_node);   }

   //! <b>Requires</b>: this_node must be in a linear list or be an empty linear list.
   //!
   //! <b>Effects</b>: Returns the number of nodes in a linear list. If the linear list
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
      } while (p);
      return result;
   }

   //! <b>Requires</b>: this_node and other_node must be nodes inserted
   //!  in linear lists or be empty linear lists.
   //!
   //! <b>Effects</b>: Moves all the nodes previously chained after this_node after other_node
   //!   and vice-versa.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void swap_trailing_nodes(node_ptr this_node, node_ptr other_node)
   {
      node_ptr this_nxt    = NodeTraits::get_next(this_node);
      node_ptr other_nxt   = NodeTraits::get_next(other_node);
      NodeTraits::set_next(this_node, other_nxt);
      NodeTraits::set_next(other_node, this_nxt);
   }

   //! <b>Effects</b>: Reverses the order of elements in the list.
   //!
   //! <b>Returns</b>: The new first node of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: This function is linear to the contained elements.
   static node_ptr reverse(node_ptr p)
   {
      if(!p) return node_ptr();
      node_ptr i = NodeTraits::get_next(p);
      node_ptr first(p);
      while(i){
         node_ptr nxti(NodeTraits::get_next(i));
         base_t::unlink_after(p);
         NodeTraits::set_next(i, first);
         first = i;
         i = nxti;
      }
      return first;
   }

   //! <b>Effects</b>: Moves the first n nodes starting at p to the end of the list.
   //!
   //! <b>Returns</b>: A pair containing the new first and last node of the list or
   //!   if there has been any movement, a null pair if n leads to no movement.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number moved positions.
   static std::pair<node_ptr, node_ptr> move_first_n_backwards(node_ptr p, std::size_t n)
   {
      std::pair<node_ptr, node_ptr> ret;
      //Null shift, or count() == 0 or 1, nothing to do
      if(!n || !p || !NodeTraits::get_next(p)){
         return ret;
      }

      node_ptr first = p;
      bool end_found = false;
      node_ptr new_last = node_ptr();
      node_ptr old_last = node_ptr();

      //Now find the new last node according to the shift count.
      //If we find 0 before finding the new last node
      //unlink p, shortcut the search now that we know the size of the list
      //and continue.
      for(std::size_t i = 1; i <= n; ++i){
         new_last = first;
         first = NodeTraits::get_next(first);
         if(first == node_ptr()){
            //Shortcut the shift with the modulo of the size of the list
            n %= i;
            if(!n)   return ret;
            old_last = new_last;
            i = 0;
            //Unlink p and continue the new first node search
            first = p;
            //unlink_after(new_last);
            end_found = true;
         }
      }

      //If the p has not been found in the previous loop, find it
      //starting in the new first node and unlink it
      if(!end_found){
         old_last = base_t::get_previous_node(first, node_ptr());
      }

      //Now link p after the new last node
      NodeTraits::set_next(old_last, p);
      NodeTraits::set_next(new_last, node_ptr());
      ret.first   = first;
      ret.second  = new_last;
      return ret;
   }

   //! <b>Effects</b>: Moves the first n nodes starting at p to the beginning of the list.
   //!
   //! <b>Returns</b>: A pair containing the new first and last node of the list or
   //!   if there has been any movement, a null pair if n leads to no movement.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements plus the number moved positions.
   static std::pair<node_ptr, node_ptr> move_first_n_forward(node_ptr p, std::size_t n)
   {
      std::pair<node_ptr, node_ptr> ret;
      //Null shift, or count() == 0 or 1, nothing to do
      if(!n || !p || !NodeTraits::get_next(p))
         return ret;

      node_ptr first  = p;

      //Iterate until p is found to know where the current last node is.
      //If the shift count is less than the size of the list, we can also obtain
      //the position of the new last node after the shift.
      node_ptr old_last(first), next_to_it, new_last(p);
      std::size_t distance = 1;
      while(!!(next_to_it = node_traits::get_next(old_last))){
         if(distance++ > n)
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
         if(!new_before_last_pos)
            return ret;

         for( new_last = p
            ; --new_before_last_pos
            ; new_last = node_traits::get_next(new_last)){
            //empty
         }
      }

      //Get the first new node
      node_ptr new_first(node_traits::get_next(new_last));
      //Now put the old beginning after the old end
      NodeTraits::set_next(old_last, p);
      NodeTraits::set_next(new_last, node_ptr());
      ret.first   = new_first;
      ret.second  = new_last;
      return ret;
   }
};

/// @cond

template<class NodeTraits>
struct get_algo<LinearSListAlgorithms, NodeTraits>
{
   typedef linear_slist_algorithms<NodeTraits> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_LINEAR_SLIST_ALGORITHMS_HPP
