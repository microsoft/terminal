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

#ifndef BOOST_INTRUSIVE_CIRCULAR_LIST_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_CIRCULAR_LIST_ALGORITHMS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <cstddef>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

//! circular_list_algorithms provides basic algorithms to manipulate nodes
//! forming a circular doubly linked list. An empty circular list is formed by a node
//! whose pointers point to itself.
//!
//! circular_list_algorithms is configured with a NodeTraits class, which encapsulates the
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
//! <tt>static node_ptr get_previous(const_node_ptr n);</tt>
//!
//! <tt>static void set_previous(node_ptr n, node_ptr prev);</tt>
//!
//! <tt>static node_ptr get_next(const_node_ptr n);</tt>
//!
//! <tt>static void set_next(node_ptr n, node_ptr next);</tt>
template<class NodeTraits>
class circular_list_algorithms
{
   public:
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef NodeTraits                           node_traits;

   //! <b>Effects</b>: Constructs an non-used list element, so that
   //! inited(this_node) == true
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void init(node_ptr this_node)
   {
      const node_ptr null_node = node_ptr();
      NodeTraits::set_next(this_node, null_node);
      NodeTraits::set_previous(this_node, null_node);
   }

   //! <b>Effects</b>: Returns true is "this_node" is in a non-used state
   //! as if it was initialized by the "init" function.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool inited(const const_node_ptr &this_node)
   {  return !NodeTraits::get_next(this_node); }

   //! <b>Effects</b>: Constructs an empty list, making this_node the only
   //!   node of the circular list:
   //!  <tt>NodeTraits::get_next(this_node) == NodeTraits::get_previous(this_node)
   //!  == this_node</tt>.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void init_header(node_ptr this_node)
   {
      NodeTraits::set_next(this_node, this_node);
      NodeTraits::set_previous(this_node, this_node);
   }


   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns true is "this_node" is the only node of a circular list:
   //!  <tt>return NodeTraits::get_next(this_node) == this_node</tt>
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool unique(const const_node_ptr &this_node)
   {
      node_ptr next = NodeTraits::get_next(this_node);
      return !next || next == this_node;
   }

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Returns the number of nodes in a circular list. If the circular list
   //!  is empty, returns 1.
   //!
   //! <b>Complexity</b>: Linear
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t count(const const_node_ptr &this_node)
   {
      std::size_t result = 0;
      const_node_ptr p = this_node;
      do{
         p = NodeTraits::get_next(p);
         ++result;
      }while (p != this_node);
      return result;
   }

   //! <b>Requires</b>: this_node must be in a circular list or be an empty circular list.
   //!
   //! <b>Effects</b>: Unlinks the node from the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr unlink(node_ptr this_node)
   {
      node_ptr next(NodeTraits::get_next(this_node));
      node_ptr prev(NodeTraits::get_previous(this_node));
      NodeTraits::set_next(prev, next);
      NodeTraits::set_previous(next, prev);
      return next;
   }

   //! <b>Requires</b>: b and e must be nodes of the same circular list or an empty range.
   //!
   //! <b>Effects</b>: Unlinks the node [b, e) from the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void unlink(node_ptr b, node_ptr e)
   {
      if (b != e) {
         node_ptr prevb(NodeTraits::get_previous(b));
         NodeTraits::set_previous(e, prevb);
         NodeTraits::set_next(prevb, e);
      }
   }

   //! <b>Requires</b>: nxt_node must be a node of a circular list.
   //!
   //! <b>Effects</b>: Links this_node before nxt_node in the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void link_before(node_ptr nxt_node, node_ptr this_node)
   {
      node_ptr prev(NodeTraits::get_previous(nxt_node));
      NodeTraits::set_previous(this_node, prev);
      NodeTraits::set_next(this_node, nxt_node);
      //nxt_node might be an alias for prev->next_
      //so use it before NodeTraits::set_next(prev, ...)
      //is called and the reference changes its value
      NodeTraits::set_previous(nxt_node, this_node);
      NodeTraits::set_next(prev, this_node);
   }

   //! <b>Requires</b>: prev_node must be a node of a circular list.
   //!
   //! <b>Effects</b>: Links this_node after prev_node in the circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void link_after(node_ptr prev_node, node_ptr this_node)
   {
      node_ptr next(NodeTraits::get_next(prev_node));
      NodeTraits::set_previous(this_node, prev_node);
      NodeTraits::set_next(this_node, next);
      //prev_node might be an alias for next->next_
      //so use it before update it before NodeTraits::set_previous(next, ...)
      //is called and the reference changes it's value
      NodeTraits::set_next(prev_node, this_node);
      NodeTraits::set_previous(next, this_node);
   }

   //! <b>Requires</b>: this_node and other_node must be nodes inserted
   //!  in circular lists or be empty circular lists.
   //!
   //! <b>Effects</b>: Swaps the position of the nodes: this_node is inserted in
   //!   other_nodes position in the second circular list and the other_node is inserted
   //!   in this_node's position in the first circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void swap_nodes(node_ptr this_node, node_ptr other_node)
   {
      if (other_node == this_node)
         return;
      bool this_inited  = inited(this_node);
      bool other_inited = inited(other_node);
      if(this_inited){
         init_header(this_node);
      }
      if(other_inited){
         init_header(other_node);
      }

      node_ptr next_this(NodeTraits::get_next(this_node));
      node_ptr prev_this(NodeTraits::get_previous(this_node));
      node_ptr next_other(NodeTraits::get_next(other_node));
      node_ptr prev_other(NodeTraits::get_previous(other_node));
      //these first two swaps must happen before the other two
      swap_prev(next_this, next_other);
      swap_next(prev_this, prev_other);
      swap_next(this_node, other_node);
      swap_prev(this_node, other_node);

      if(this_inited){
         init(other_node);
      }
      if(other_inited){
         init(this_node);
      }
   }

   //! <b>Requires</b>: b and e must be nodes of the same circular list or an empty range.
   //!   and p must be a node of a different circular list or may not be an iterator in
   //    [b, e).
   //!
   //! <b>Effects</b>: Removes the nodes from [b, e) range from their circular list and inserts
   //!   them before p in p's circular list.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void transfer(node_ptr p, node_ptr b, node_ptr e)
   {
      if (b != e) {
         node_ptr prev_p(NodeTraits::get_previous(p));
         node_ptr prev_b(NodeTraits::get_previous(b));
         node_ptr prev_e(NodeTraits::get_previous(e));
         NodeTraits::set_next(prev_e, p);
         NodeTraits::set_previous(p, prev_e);
         NodeTraits::set_next(prev_b, e);
         NodeTraits::set_previous(e, prev_b);
         NodeTraits::set_next(prev_p, b);
         NodeTraits::set_previous(b, prev_p);
      }
   }

   //! <b>Requires</b>: i must a node of a circular list
   //!   and p must be a node of a different circular list.
   //!
   //! <b>Effects</b>: Removes the node i from its circular list and inserts
   //!   it before p in p's circular list.
   //!   If p == i or p == NodeTraits::get_next(i), this function is a null operation.
   //!
   //! <b>Complexity</b>: Constant
   //!
   //! <b>Throws</b>: Nothing.
   static void transfer(node_ptr p, node_ptr i)
   {
      node_ptr n(NodeTraits::get_next(i));
      if(n != p && i != p){
         node_ptr prev_p(NodeTraits::get_previous(p));
         node_ptr prev_i(NodeTraits::get_previous(i));
         NodeTraits::set_next(prev_p, i);
         NodeTraits::set_previous(i, prev_p);
         NodeTraits::set_next(i, p);
         NodeTraits::set_previous(p, i);
         NodeTraits::set_previous(n, prev_i);
         NodeTraits::set_next(prev_i, n);

      }
   }

   //! <b>Effects</b>: Reverses the order of elements in the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: This function is linear time.
   static void reverse(node_ptr p)
   {
      node_ptr f(NodeTraits::get_next(p));
      node_ptr i(NodeTraits::get_next(f)), e(p);

      while(i != e) {
         node_ptr n = i;
         i = NodeTraits::get_next(i);
         transfer(f, n, i);
         f = n;
      }
   }

   //! <b>Effects</b>: Moves the node p n positions towards the end of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of moved positions.
   static void move_backwards(node_ptr p, std::size_t n)
   {
      //Null shift, nothing to do
      if(!n) return;
      node_ptr first  = NodeTraits::get_next(p);
      //size() == 0 or 1, nothing to do
      if(first == NodeTraits::get_previous(p)) return;
      unlink(p);
      //Now get the new first node
      while(n--){
         first = NodeTraits::get_next(first);
      }
      link_before(first, p);
   }

   //! <b>Effects</b>: Moves the node p n positions towards the beginning of the list.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of moved positions.
   static void move_forward(node_ptr p, std::size_t n)
   {
      //Null shift, nothing to do
      if(!n)   return;
      node_ptr last  = NodeTraits::get_previous(p);
      //size() == 0 or 1, nothing to do
      if(last == NodeTraits::get_next(p))   return;

      unlink(p);
      //Now get the new last node
      while(n--){
         last = NodeTraits::get_previous(last);
      }
      link_after(last, p);
   }

   //! <b>Requires</b>: f and l must be in a circular list.
   //!
   //! <b>Effects</b>: Returns the number of nodes in the range [f, l).
   //!
   //! <b>Complexity</b>: Linear
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t distance(const const_node_ptr &f, const const_node_ptr &l)
   {
      const_node_ptr i(f);
      std::size_t result = 0;
      while(i != l){
         i = NodeTraits::get_next(i);
         ++result;
      }
      return result;
   }

   struct stable_partition_info
   {
      std::size_t num_1st_partition;
      std::size_t num_2nd_partition;
      node_ptr    beg_2st_partition;
   };

   template<class Pred>
   static void stable_partition(node_ptr beg, node_ptr end, Pred pred, stable_partition_info &info)
   {
      node_ptr bcur = node_traits::get_previous(beg);
      node_ptr cur  = beg;
      node_ptr new_f = end;

      std::size_t num1 = 0, num2 = 0;
      while(cur != end){
         if(pred(cur)){
            ++num1;
            bcur = cur;
            cur  = node_traits::get_next(cur);
         }
         else{
            ++num2;
            node_ptr last_to_remove = bcur;
            new_f = cur;
            bcur = cur;
            cur  = node_traits::get_next(cur);
            BOOST_TRY{
               //Main loop
               while(cur != end){
                  if(pred(cur)){ //Might throw
                     ++num1;
                     //Process current node
                     node_traits::set_next    (last_to_remove, cur);
                     node_traits::set_previous(cur, last_to_remove);
                     last_to_remove = cur;
                     node_ptr nxt = node_traits::get_next(cur);
                     node_traits::set_next    (bcur, nxt);
                     node_traits::set_previous(nxt, bcur);
                     cur = nxt;
                  }
                  else{
                     ++num2;
                     bcur = cur;
                     cur  = node_traits::get_next(cur);
                  }
               }
            }
            BOOST_CATCH(...){
               node_traits::set_next    (last_to_remove, new_f);
               node_traits::set_previous(new_f, last_to_remove);
               BOOST_RETHROW;
            }
            BOOST_CATCH_END
            node_traits::set_next(last_to_remove, new_f);
            node_traits::set_previous(new_f, last_to_remove);
            break;
         }
      }
      info.num_1st_partition = num1;
      info.num_2nd_partition = num2;
      info.beg_2st_partition = new_f;
   }

   private:
   BOOST_INTRUSIVE_FORCEINLINE static void swap_prev(node_ptr this_node, node_ptr other_node)
   {
      node_ptr temp(NodeTraits::get_previous(this_node));
      NodeTraits::set_previous(this_node, NodeTraits::get_previous(other_node));
      NodeTraits::set_previous(other_node, temp);
   }

   BOOST_INTRUSIVE_FORCEINLINE static void swap_next(node_ptr this_node, node_ptr other_node)
   {
      node_ptr temp(NodeTraits::get_next(this_node));
      NodeTraits::set_next(this_node, NodeTraits::get_next(other_node));
      NodeTraits::set_next(other_node, temp);
   }
};

/// @cond

template<class NodeTraits>
struct get_algo<CircularListAlgorithms, NodeTraits>
{
   typedef circular_list_algorithms<NodeTraits> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_CIRCULAR_LIST_ALGORITHMS_HPP
