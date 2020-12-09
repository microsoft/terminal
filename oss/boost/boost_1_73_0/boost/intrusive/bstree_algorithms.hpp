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

#ifndef BOOST_INTRUSIVE_BSTREE_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_BSTREE_ALGORITHMS_HPP

#include <cstddef>
#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>
#include <boost/intrusive/detail/bstree_algorithms_base.hpp>
#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/detail/uncast.hpp>
#include <boost/intrusive/detail/math.hpp>
#include <boost/intrusive/detail/algo_type.hpp>

#include <boost/intrusive/detail/minimal_pair_header.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

/// @cond

//! This type is the information that will be filled by insert_unique_check
template <class NodePtr>
struct insert_commit_data_t
{
   BOOST_INTRUSIVE_FORCEINLINE insert_commit_data_t()
      : link_left(false), node()
   {}
   bool     link_left;
   NodePtr  node;
};

template <class NodePtr>
struct data_for_rebalance_t
{
   NodePtr  x;
   NodePtr  x_parent;
   NodePtr  y;
};

namespace detail {

template<class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct bstree_node_checker
   : public ExtraChecker
{
   typedef ExtraChecker                            base_checker_t;
   typedef ValueTraits                             value_traits;
   typedef typename value_traits::node_traits      node_traits;
   typedef typename node_traits::const_node_ptr    const_node_ptr;

   struct return_type
      : public base_checker_t::return_type
   {
      BOOST_INTRUSIVE_FORCEINLINE return_type()
         : min_key_node_ptr(const_node_ptr()), max_key_node_ptr(const_node_ptr()), node_count(0)
      {}

      const_node_ptr min_key_node_ptr;
      const_node_ptr max_key_node_ptr;
      size_t   node_count;
   };

   BOOST_INTRUSIVE_FORCEINLINE bstree_node_checker(const NodePtrCompare& comp, ExtraChecker extra_checker)
      : base_checker_t(extra_checker), comp_(comp)
   {}

   void operator () (const const_node_ptr& p,
                     const return_type& check_return_left, const return_type& check_return_right,
                     return_type& check_return)
   {
      if (check_return_left.max_key_node_ptr)
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!comp_(p, check_return_left.max_key_node_ptr));
      if (check_return_right.min_key_node_ptr)
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!comp_(check_return_right.min_key_node_ptr, p));
      check_return.min_key_node_ptr = node_traits::get_left(p)? check_return_left.min_key_node_ptr : p;
      check_return.max_key_node_ptr = node_traits::get_right(p)? check_return_right.max_key_node_ptr : p;
      check_return.node_count = check_return_left.node_count + check_return_right.node_count + 1;
      base_checker_t::operator()(p, check_return_left, check_return_right, check_return);
   }

   const NodePtrCompare comp_;
};

} // namespace detail

/// @endcond



//!   This is an implementation of a binary search tree.
//!   A node in the search tree has references to its children and its parent. This
//!   is to allow traversal of the whole tree from a given node making the
//!   implementation of iterator a pointer to a node.
//!   At the top of the tree a node is used specially. This node's parent pointer
//!   is pointing to the root of the tree. Its left pointer points to the
//!   leftmost node in the tree and the right pointer to the rightmost one.
//!   This node is used to represent the end-iterator.
//!
//!                                            +---------+
//!       header------------------------------>|         |
//!                                            |         |
//!                   +----------(left)--------|         |--------(right)---------+
//!                   |                        +---------+                        |
//!                   |                             |                             |
//!                   |                             | (parent)                    |
//!                   |                             |                             |
//!                   |                             |                             |
//!                   |                        +---------+                        |
//!    root of tree ..|......................> |         |                        |
//!                   |                        |    D    |                        |
//!                   |                        |         |                        |
//!                   |                +-------+---------+-------+                |
//!                   |                |                         |                |
//!                   |                |                         |                |
//!                   |                |                         |                |
//!                   |                |                         |                |
//!                   |                |                         |                |
//!                   |          +---------+                 +---------+          |
//!                   |          |         |                 |         |          |
//!                   |          |    B    |                 |    F    |          |
//!                   |          |         |                 |         |          |
//!                   |       +--+---------+--+           +--+---------+--+       |
//!                   |       |               |           |               |       |
//!                   |       |               |           |               |       |
//!                   |       |               |           |               |       |
//!                   |   +---+-----+   +-----+---+   +---+-----+   +-----+---+   |
//!                   +-->|         |   |         |   |         |   |         |<--+
//!                       |    A    |   |    C    |   |    E    |   |    G    |
//!                       |         |   |         |   |         |   |         |
//!                       +---------+   +---------+   +---------+   +---------+
//!
//! bstree_algorithms is configured with a NodeTraits class, which encapsulates the
//! information about the node to be manipulated. NodeTraits must support the
//! following interface:
//!
//! <b>Typedefs</b>:
//!
//! <tt>node</tt>: The type of the node that forms the binary search tree
//!
//! <tt>node_ptr</tt>: A pointer to a node
//!
//! <tt>const_node_ptr</tt>: A pointer to a const node
//!
//! <b>Static functions</b>:
//!
//! <tt>static node_ptr get_parent(const_node_ptr n);</tt>
//!
//! <tt>static void set_parent(node_ptr n, node_ptr parent);</tt>
//!
//! <tt>static node_ptr get_left(const_node_ptr n);</tt>
//!
//! <tt>static void set_left(node_ptr n, node_ptr left);</tt>
//!
//! <tt>static node_ptr get_right(const_node_ptr n);</tt>
//!
//! <tt>static void set_right(node_ptr n, node_ptr right);</tt>
template<class NodeTraits>
class bstree_algorithms : public bstree_algorithms_base<NodeTraits>
{
   public:
   typedef typename NodeTraits::node            node;
   typedef NodeTraits                           node_traits;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef insert_commit_data_t<node_ptr>       insert_commit_data;
   typedef data_for_rebalance_t<node_ptr>       data_for_rebalance;

   /// @cond
   typedef bstree_algorithms<NodeTraits>        this_type;
   typedef bstree_algorithms_base<NodeTraits>   base_type;
   private:
   template<class Disposer>
   struct dispose_subtree_disposer
   {
      BOOST_INTRUSIVE_FORCEINLINE dispose_subtree_disposer(Disposer &disp, const node_ptr & subtree)
         : disposer_(&disp), subtree_(subtree)
      {}

      BOOST_INTRUSIVE_FORCEINLINE void release()
      {  disposer_ = 0;  }

      BOOST_INTRUSIVE_FORCEINLINE ~dispose_subtree_disposer()
      {
         if(disposer_){
            dispose_subtree(subtree_, *disposer_);
         }
      }
      Disposer *disposer_;
      const node_ptr subtree_;
   };

   /// @endcond

   public:
   //! <b>Requires</b>: 'header' is the header node of a tree.
   //!
   //! <b>Effects</b>: Returns the first node of the tree, the header if the tree is empty.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr begin_node(const const_node_ptr & header)
   {  return node_traits::get_left(header);   }

   //! <b>Requires</b>: 'header' is the header node of a tree.
   //!
   //! <b>Effects</b>: Returns the header of the tree.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr end_node(const const_node_ptr & header)
   {  return detail::uncast(header);   }

   //! <b>Requires</b>: 'header' is the header node of a tree.
   //!
   //! <b>Effects</b>: Returns the root of the tree if any, header otherwise
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr root_node(const const_node_ptr & header)
   {
      node_ptr p = node_traits::get_parent(header);
      return p ? p : detail::uncast(header);
   }

   //! <b>Requires</b>: 'node' is a node of the tree or a node initialized
   //!   by init(...) or init_node.
   //!
   //! <b>Effects</b>: Returns true if the node is initialized by init() or init_node().
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool unique(const const_node_ptr & node)
   { return !NodeTraits::get_parent(node); }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   //! <b>Requires</b>: 'node' is a node of the tree or a header node.
   //!
   //! <b>Effects</b>: Returns the header of the tree.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr get_header(const const_node_ptr & node);
   #endif

   //! <b>Requires</b>: node1 and node2 can't be header nodes
   //!  of two trees.
   //!
   //! <b>Effects</b>: Swaps two nodes. After the function node1 will be inserted
   //!   in the position node2 before the function. node2 will be inserted in the
   //!   position node1 had before the function.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: This function will break container ordering invariants if
   //!   node1 and node2 are not equivalent according to the ordering rules.
   //!
   //!Experimental function
   static void swap_nodes(node_ptr node1, node_ptr node2)
   {
      if(node1 == node2)
         return;

      node_ptr header1(base_type::get_header(node1)), header2(base_type::get_header(node2));
      swap_nodes(node1, header1, node2, header2);
   }

   //! <b>Requires</b>: node1 and node2 can't be header nodes
   //!  of two trees with header header1 and header2.
   //!
   //! <b>Effects</b>: Swaps two nodes. After the function node1 will be inserted
   //!   in the position node2 before the function. node2 will be inserted in the
   //!   position node1 had before the function.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: This function will break container ordering invariants if
   //!   node1 and node2 are not equivalent according to the ordering rules.
   //!
   //!Experimental function
   static void swap_nodes(node_ptr node1, node_ptr header1, node_ptr node2, node_ptr header2)
   {
      if(node1 == node2)
         return;

      //node1 and node2 must not be header nodes
      //BOOST_INTRUSIVE_INVARIANT_ASSERT((header1 != node1 && header2 != node2));
      if(header1 != header2){
         //Update header1 if necessary
         if(node1 == NodeTraits::get_left(header1)){
            NodeTraits::set_left(header1, node2);
         }

         if(node1 == NodeTraits::get_right(header1)){
            NodeTraits::set_right(header1, node2);
         }

         if(node1 == NodeTraits::get_parent(header1)){
            NodeTraits::set_parent(header1, node2);
         }

         //Update header2 if necessary
         if(node2 == NodeTraits::get_left(header2)){
            NodeTraits::set_left(header2, node1);
         }

         if(node2 == NodeTraits::get_right(header2)){
            NodeTraits::set_right(header2, node1);
         }

         if(node2 == NodeTraits::get_parent(header2)){
            NodeTraits::set_parent(header2, node1);
         }
      }
      else{
         //If both nodes are from the same tree
         //Update header if necessary
         if(node1 == NodeTraits::get_left(header1)){
            NodeTraits::set_left(header1, node2);
         }
         else if(node2 == NodeTraits::get_left(header2)){
            NodeTraits::set_left(header2, node1);
         }

         if(node1 == NodeTraits::get_right(header1)){
            NodeTraits::set_right(header1, node2);
         }
         else if(node2 == NodeTraits::get_right(header2)){
            NodeTraits::set_right(header2, node1);
         }

         if(node1 == NodeTraits::get_parent(header1)){
            NodeTraits::set_parent(header1, node2);
         }
         else if(node2 == NodeTraits::get_parent(header2)){
            NodeTraits::set_parent(header2, node1);
         }

         //Adjust data in nodes to be swapped
         //so that final link swap works as expected
         if(node1 == NodeTraits::get_parent(node2)){
            NodeTraits::set_parent(node2, node2);

            if(node2 == NodeTraits::get_right(node1)){
               NodeTraits::set_right(node1, node1);
            }
            else{
               NodeTraits::set_left(node1, node1);
            }
         }
         else if(node2 == NodeTraits::get_parent(node1)){
            NodeTraits::set_parent(node1, node1);

            if(node1 == NodeTraits::get_right(node2)){
               NodeTraits::set_right(node2, node2);
            }
            else{
               NodeTraits::set_left(node2, node2);
            }
         }
      }

      //Now swap all the links
      node_ptr temp;
      //swap left link
      temp = NodeTraits::get_left(node1);
      NodeTraits::set_left(node1, NodeTraits::get_left(node2));
      NodeTraits::set_left(node2, temp);
      //swap right link
      temp = NodeTraits::get_right(node1);
      NodeTraits::set_right(node1, NodeTraits::get_right(node2));
      NodeTraits::set_right(node2, temp);
      //swap parent link
      temp = NodeTraits::get_parent(node1);
      NodeTraits::set_parent(node1, NodeTraits::get_parent(node2));
      NodeTraits::set_parent(node2, temp);

      //Now adjust adjacent nodes for newly inserted node 1
      if((temp = NodeTraits::get_left(node1))){
         NodeTraits::set_parent(temp, node1);
      }
      if((temp = NodeTraits::get_right(node1))){
         NodeTraits::set_parent(temp, node1);
      }
      if((temp = NodeTraits::get_parent(node1)) &&
         //The header has been already updated so avoid it
         temp != header2){
         if(NodeTraits::get_left(temp) == node2){
            NodeTraits::set_left(temp, node1);
         }
         if(NodeTraits::get_right(temp) == node2){
            NodeTraits::set_right(temp, node1);
         }
      }
      //Now adjust adjacent nodes for newly inserted node 2
      if((temp = NodeTraits::get_left(node2))){
         NodeTraits::set_parent(temp, node2);
      }
      if((temp = NodeTraits::get_right(node2))){
         NodeTraits::set_parent(temp, node2);
      }
      if((temp = NodeTraits::get_parent(node2)) &&
         //The header has been already updated so avoid it
         temp != header1){
         if(NodeTraits::get_left(temp) == node1){
            NodeTraits::set_left(temp, node2);
         }
         if(NodeTraits::get_right(temp) == node1){
            NodeTraits::set_right(temp, node2);
         }
      }
   }

   //! <b>Requires</b>: node_to_be_replaced must be inserted in a tree
   //!   and new_node must not be inserted in a tree.
   //!
   //! <b>Effects</b>: Replaces node_to_be_replaced in its position in the
   //!   tree with new_node. The tree does not need to be rebalanced
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: This function will break container ordering invariants if
   //!   new_node is not equivalent to node_to_be_replaced according to the
   //!   ordering rules. This function is faster than erasing and inserting
   //!   the node, since no rebalancing and comparison is needed. Experimental function
   BOOST_INTRUSIVE_FORCEINLINE static void replace_node(node_ptr node_to_be_replaced, node_ptr new_node)
   {
      if(node_to_be_replaced == new_node)
         return;
      replace_node(node_to_be_replaced, base_type::get_header(node_to_be_replaced), new_node);
   }

   //! <b>Requires</b>: node_to_be_replaced must be inserted in a tree
   //!   with header "header" and new_node must not be inserted in a tree.
   //!
   //! <b>Effects</b>: Replaces node_to_be_replaced in its position in the
   //!   tree with new_node. The tree does not need to be rebalanced
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: This function will break container ordering invariants if
   //!   new_node is not equivalent to node_to_be_replaced according to the
   //!   ordering rules. This function is faster than erasing and inserting
   //!   the node, since no rebalancing or comparison is needed. Experimental function
   static void replace_node(node_ptr node_to_be_replaced, node_ptr header, node_ptr new_node)
   {
      if(node_to_be_replaced == new_node)
         return;

      //Update header if necessary
      if(node_to_be_replaced == NodeTraits::get_left(header)){
         NodeTraits::set_left(header, new_node);
      }

      if(node_to_be_replaced == NodeTraits::get_right(header)){
         NodeTraits::set_right(header, new_node);
      }

      if(node_to_be_replaced == NodeTraits::get_parent(header)){
         NodeTraits::set_parent(header, new_node);
      }

      //Now set data from the original node
      node_ptr temp;
      NodeTraits::set_left(new_node, NodeTraits::get_left(node_to_be_replaced));
      NodeTraits::set_right(new_node, NodeTraits::get_right(node_to_be_replaced));
      NodeTraits::set_parent(new_node, NodeTraits::get_parent(node_to_be_replaced));

      //Now adjust adjacent nodes for newly inserted node
      if((temp = NodeTraits::get_left(new_node))){
         NodeTraits::set_parent(temp, new_node);
      }
      if((temp = NodeTraits::get_right(new_node))){
         NodeTraits::set_parent(temp, new_node);
      }
      if((temp = NodeTraits::get_parent(new_node)) &&
         //The header has been already updated so avoid it
         temp != header){
         if(NodeTraits::get_left(temp) == node_to_be_replaced){
            NodeTraits::set_left(temp, new_node);
         }
         if(NodeTraits::get_right(temp) == node_to_be_replaced){
            NodeTraits::set_right(temp, new_node);
         }
      }
   }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   //! <b>Requires</b>: 'node' is a node from the tree except the header.
   //!
   //! <b>Effects</b>: Returns the next node of the tree.
   //!
   //! <b>Complexity</b>: Average constant time.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr next_node(const node_ptr & node);

   //! <b>Requires</b>: 'node' is a node from the tree except the leftmost node.
   //!
   //! <b>Effects</b>: Returns the previous node of the tree.
   //!
   //! <b>Complexity</b>: Average constant time.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr prev_node(const node_ptr & node);

   //! <b>Requires</b>: 'node' is a node of a tree but not the header.
   //!
   //! <b>Effects</b>: Returns the minimum node of the subtree starting at p.
   //!
   //! <b>Complexity</b>: Logarithmic to the size of the subtree.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr minimum(node_ptr node);

   //! <b>Requires</b>: 'node' is a node of a tree but not the header.
   //!
   //! <b>Effects</b>: Returns the maximum node of the subtree starting at p.
   //!
   //! <b>Complexity</b>: Logarithmic to the size of the subtree.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr maximum(node_ptr node);
   #endif

   //! <b>Requires</b>: 'node' must not be part of any tree.
   //!
   //! <b>Effects</b>: After the function unique(node) == true.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Nodes</b>: If node is inserted in a tree, this function corrupts the tree.
   BOOST_INTRUSIVE_FORCEINLINE static void init(node_ptr node)
   {
      NodeTraits::set_parent(node, node_ptr());
      NodeTraits::set_left(node, node_ptr());
      NodeTraits::set_right(node, node_ptr());
   }

   //! <b>Effects</b>: Returns true if node is in the same state as if called init(node)
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool inited(const const_node_ptr & node)
   {
      return !NodeTraits::get_parent(node) &&
             !NodeTraits::get_left(node)   &&
             !NodeTraits::get_right(node)  ;
   }

   //! <b>Requires</b>: node must not be part of any tree.
   //!
   //! <b>Effects</b>: Initializes the header to represent an empty tree.
   //!   unique(header) == true.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Nodes</b>: If node is inserted in a tree, this function corrupts the tree.
   BOOST_INTRUSIVE_FORCEINLINE static void init_header(node_ptr header)
   {
      NodeTraits::set_parent(header, node_ptr());
      NodeTraits::set_left(header, header);
      NodeTraits::set_right(header, header);
   }

   //! <b>Requires</b>: "disposer" must be an object function
   //!   taking a node_ptr parameter and shouldn't throw.
   //!
   //! <b>Effects</b>: Empties the target tree calling
   //!   <tt>void disposer::operator()(const node_ptr &)</tt> for every node of the tree
   //!    except the header.
   //!
   //! <b>Complexity</b>: Linear to the number of element of the source tree plus the.
   //!   number of elements of tree target tree when calling this function.
   //!
   //! <b>Throws</b>: If cloner functor throws. If this happens target nodes are disposed.
   template<class Disposer>
   static void clear_and_dispose(const node_ptr & header, Disposer disposer)
   {
      node_ptr source_root = NodeTraits::get_parent(header);
      if(!source_root)
         return;
      dispose_subtree(source_root, disposer);
      init_header(header);
   }

   //! <b>Requires</b>: header is the header of a tree.
   //!
   //! <b>Effects</b>: Unlinks the leftmost node from the tree, and
   //!   updates the header link to the new leftmost node.
   //!
   //! <b>Complexity</b>: Average complexity is constant time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Notes</b>: This function breaks the tree and the tree can
   //!   only be used for more unlink_leftmost_without_rebalance calls.
   //!   This function is normally used to achieve a step by step
   //!   controlled destruction of the tree.
   static node_ptr unlink_leftmost_without_rebalance(node_ptr header)
   {
      node_ptr leftmost = NodeTraits::get_left(header);
      if (leftmost == header)
         return node_ptr();
      node_ptr leftmost_parent(NodeTraits::get_parent(leftmost));
      node_ptr leftmost_right (NodeTraits::get_right(leftmost));
      bool is_root = leftmost_parent == header;

      if (leftmost_right){
         NodeTraits::set_parent(leftmost_right, leftmost_parent);
         NodeTraits::set_left(header, base_type::minimum(leftmost_right));

         if (is_root)
            NodeTraits::set_parent(header, leftmost_right);
         else
            NodeTraits::set_left(NodeTraits::get_parent(header), leftmost_right);
      }
      else if (is_root){
         NodeTraits::set_parent(header, node_ptr());
         NodeTraits::set_left(header,  header);
         NodeTraits::set_right(header, header);
      }
      else{
         NodeTraits::set_left(leftmost_parent, node_ptr());
         NodeTraits::set_left(header, leftmost_parent);
      }
      return leftmost;
   }

   //! <b>Requires</b>: node is a node of the tree but it's not the header.
   //!
   //! <b>Effects</b>: Returns the number of nodes of the subtree.
   //!
   //! <b>Complexity</b>: Linear time.
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t size(const const_node_ptr & header)
   {
      node_ptr beg(begin_node(header));
      node_ptr end(end_node(header));
      std::size_t i = 0;
      for(;beg != end; beg = base_type::next_node(beg)) ++i;
      return i;
   }

   //! <b>Requires</b>: header1 and header2 must be the header nodes
   //!  of two trees.
   //!
   //! <b>Effects</b>: Swaps two trees. After the function header1 will contain
   //!   links to the second tree and header2 will have links to the first tree.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   static void swap_tree(node_ptr header1, node_ptr header2)
   {
      if(header1 == header2)
         return;

      node_ptr tmp;

      //Parent swap
      tmp = NodeTraits::get_parent(header1);
      NodeTraits::set_parent(header1, NodeTraits::get_parent(header2));
      NodeTraits::set_parent(header2, tmp);
      //Left swap
      tmp = NodeTraits::get_left(header1);
      NodeTraits::set_left(header1, NodeTraits::get_left(header2));
      NodeTraits::set_left(header2, tmp);
      //Right swap
      tmp = NodeTraits::get_right(header1);
      NodeTraits::set_right(header1, NodeTraits::get_right(header2));
      NodeTraits::set_right(header2, tmp);

      //Now test parent
      node_ptr h1_parent(NodeTraits::get_parent(header1));
      if(h1_parent){
         NodeTraits::set_parent(h1_parent, header1);
      }
      else{
         NodeTraits::set_left(header1, header1);
         NodeTraits::set_right(header1, header1);
      }

      node_ptr h2_parent(NodeTraits::get_parent(header2));
      if(h2_parent){
         NodeTraits::set_parent(h2_parent, header2);
      }
      else{
         NodeTraits::set_left(header2, header2);
         NodeTraits::set_right(header2, header2);
      }
   }

   #if defined(BOOST_INTRUSIVE_DOXYGEN_INVOKED)
   //! <b>Requires</b>: p is a node of a tree.
   //!
   //! <b>Effects</b>: Returns true if p is the header of the tree.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   static bool is_header(const const_node_ptr & p);
   #endif

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns a node_ptr to the first element that is equivalent to
   //!   "key" according to "comp" or "header" if that element does not exist.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr find
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      node_ptr end = detail::uncast(header);
      node_ptr y = lower_bound(header, key, comp);
      return (y == end || comp(key, y)) ? end : y;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!   'lower_key' must not be greater than 'upper_key' according to 'comp'. If
   //!   'lower_key' == 'upper_key', ('left_closed' || 'right_closed') must be true.
   //!
   //! <b>Effects</b>: Returns an a pair with the following criteria:
   //!
   //!   first = lower_bound(lower_key) if left_closed, upper_bound(lower_key) otherwise
   //!
   //!   second = upper_bound(upper_key) if right_closed, lower_bound(upper_key) otherwise
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   //!
   //! <b>Note</b>: This function can be more efficient than calling upper_bound
   //!   and lower_bound for lower_key and upper_key.
   //!
   //! <b>Note</b>: Experimental function, the interface might change.
   template< class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> bounded_range
      ( const const_node_ptr & header
      , const KeyType &lower_key
      , const KeyType &upper_key
      , KeyNodePtrCompare comp
      , bool left_closed
      , bool right_closed)
   {
      node_ptr y = detail::uncast(header);
      node_ptr x = NodeTraits::get_parent(header);

      while(x){
         //If x is less than lower_key the target
         //range is on the right part
         if(comp(x, lower_key)){
            //Check for invalid input range
            BOOST_INTRUSIVE_INVARIANT_ASSERT(comp(x, upper_key));
            x = NodeTraits::get_right(x);
         }
         //If the upper_key is less than x, the target
         //range is on the left part
         else if(comp(upper_key, x)){
            y = x;
            x = NodeTraits::get_left(x);
         }
         else{
            //x is inside the bounded range(lower_key <= x <= upper_key),
            //so we must split lower and upper searches
            //
            //Sanity check: if lower_key and upper_key are equal, then both left_closed and right_closed can't be false
            BOOST_INTRUSIVE_INVARIANT_ASSERT(left_closed || right_closed || comp(lower_key, x) || comp(x, upper_key));
            return std::pair<node_ptr,node_ptr>(
               left_closed
                  //If left_closed, then comp(x, lower_key) is already the lower_bound
                  //condition so we save one comparison and go to the next level
                  //following traditional lower_bound algo
                  ? lower_bound_loop(NodeTraits::get_left(x), x, lower_key, comp)
                  //If left-open, comp(x, lower_key) is not the upper_bound algo
                  //condition so we must recheck current 'x' node with upper_bound algo
                  : upper_bound_loop(x, y, lower_key, comp)
            ,
               right_closed
                  //If right_closed, then comp(upper_key, x) is already the upper_bound
                  //condition so we can save one comparison and go to the next level
                  //following lower_bound algo
                  ? upper_bound_loop(NodeTraits::get_right(x), y, upper_key, comp)
                  //If right-open, comp(upper_key, x) is not the lower_bound algo
                  //condition so we must recheck current 'x' node with lower_bound algo
                  : lower_bound_loop(x, y, upper_key, comp)
            );
         }
      }
      return std::pair<node_ptr,node_ptr> (y, y);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns the number of elements with a key equivalent to "key"
   //!   according to "comp".
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   static std::size_t count
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      std::pair<node_ptr, node_ptr> ret = equal_range(header, key, comp);
      std::size_t n = 0;
      while(ret.first != ret.second){
         ++n;
         ret.first = base_type::next_node(ret.first);
      }
      return n;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns an a pair of node_ptr delimiting a range containing
   //!   all elements that are equivalent to "key" according to "comp" or an
   //!   empty range that indicates the position where those elements would be
   //!   if there are no equivalent elements.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   BOOST_INTRUSIVE_FORCEINLINE static std::pair<node_ptr, node_ptr> equal_range
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      return bounded_range(header, key, key, comp, true, true);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns an a pair of node_ptr delimiting a range containing
   //!   the first element that is equivalent to "key" according to "comp" or an
   //!   empty range that indicates the position where that element would be
   //!   if there are no equivalent elements.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> lower_bound_range
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      node_ptr const lb(lower_bound(header, key, comp));
      std::pair<node_ptr, node_ptr> ret_ii(lb, lb);
      if(lb != header && !comp(key, lb)){
         ret_ii.second = base_type::next_node(ret_ii.second);
      }
      return ret_ii;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns a node_ptr to the first element that is
   //!   not less than "key" according to "comp" or "header" if that element does
   //!   not exist.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr lower_bound
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      return lower_bound_loop(NodeTraits::get_parent(header), detail::uncast(header), key, comp);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. KeyNodePtrCompare can compare KeyType with tree's node_ptrs.
   //!
   //! <b>Effects</b>: Returns a node_ptr to the first element that is greater
   //!   than "key" according to "comp" or "header" if that element does not exist.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class KeyType, class KeyNodePtrCompare>
   BOOST_INTRUSIVE_FORCEINLINE static node_ptr upper_bound
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp)
   {
      return upper_bound_loop(NodeTraits::get_parent(header), detail::uncast(header), key, comp);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "commit_data" must have been obtained from a previous call to
   //!   "insert_unique_check". No objects should have been inserted or erased
   //!   from the set between the "insert_unique_check" that filled "commit_data"
   //!   and the call to "insert_commit".
   //!
   //!
   //! <b>Effects</b>: Inserts new_node in the set using the information obtained
   //!   from the "commit_data" that a previous "insert_check" filled.
   //!
   //! <b>Complexity</b>: Constant time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Notes</b>: This function has only sense if a "insert_unique_check" has been
   //!   previously executed to fill "commit_data". No value should be inserted or
   //!   erased between the "insert_check" and "insert_commit" calls.
   BOOST_INTRUSIVE_FORCEINLINE static void insert_unique_commit
      (node_ptr header, node_ptr new_value, const insert_commit_data &commit_data)
   {  return insert_commit(header, new_value, commit_data); }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares KeyType with a node_ptr.
   //!
   //! <b>Effects</b>: Checks if there is an equivalent node to "key" in the
   //!   tree according to "comp" and obtains the needed information to realize
   //!   a constant-time node insertion if there is no equivalent node.
   //!
   //! <b>Returns</b>: If there is an equivalent value
   //!   returns a pair containing a node_ptr to the already present node
   //!   and false. If there is not equivalent key can be inserted returns true
   //!   in the returned pair's boolean and fills "commit_data" that is meant to
   //!   be used with the "insert_commit" function to achieve a constant-time
   //!   insertion function.
   //!
   //! <b>Complexity</b>: Average complexity is at most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   //!
   //! <b>Notes</b>: This function is used to improve performance when constructing
   //!   a node is expensive and the user does not want to have two equivalent nodes
   //!   in the tree: if there is an equivalent value
   //!   the constructed object must be discarded. Many times, the part of the
   //!   node that is used to impose the order is much cheaper to construct
   //!   than the node and this function offers the possibility to use that part
   //!   to check if the insertion will be successful.
   //!
   //!   If the check is successful, the user can construct the node and use
   //!   "insert_commit" to insert the node in constant-time. This gives a total
   //!   logarithmic complexity to the insertion: check(O(log(N)) + commit(O(1)).
   //!
   //!   "commit_data" remains valid for a subsequent "insert_unique_commit" only
   //!   if no more objects are inserted or erased from the set.
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      (const const_node_ptr & header, const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      std::size_t depth = 0;
      node_ptr h(detail::uncast(header));
      node_ptr y(h);
      node_ptr x(NodeTraits::get_parent(y));
      node_ptr prev = node_ptr();

      //Find the upper bound, cache the previous value and if we should
      //store it in the left or right node
      bool left_child = true;
      while(x){
         ++depth;
         y = x;
         x = (left_child = comp(key, x)) ?
               NodeTraits::get_left(x) : (prev = y, NodeTraits::get_right(x));
      }

      if(pdepth)  *pdepth = depth;

      //Since we've found the upper bound there is no other value with the same key if:
      //    - There is no previous node
      //    - The previous node is less than the key
      const bool not_present = !prev || comp(prev, key);
      if(not_present){
         commit_data.link_left = left_child;
         commit_data.node      = y;
      }
      return std::pair<node_ptr, bool>(prev, not_present);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   KeyNodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares KeyType with a node_ptr.
   //!   "hint" is node from the "header"'s tree.
   //!
   //! <b>Effects</b>: Checks if there is an equivalent node to "key" in the
   //!   tree according to "comp" using "hint" as a hint to where it should be
   //!   inserted and obtains the needed information to realize
   //!   a constant-time node insertion if there is no equivalent node.
   //!   If "hint" is the upper_bound the function has constant time
   //!   complexity (two comparisons in the worst case).
   //!
   //! <b>Returns</b>: If there is an equivalent value
   //!   returns a pair containing a node_ptr to the already present node
   //!   and false. If there is not equivalent key can be inserted returns true
   //!   in the returned pair's boolean and fills "commit_data" that is meant to
   //!   be used with the "insert_commit" function to achieve a constant-time
   //!   insertion function.
   //!
   //! <b>Complexity</b>: Average complexity is at most logarithmic, but it is
   //!   amortized constant time if new_node should be inserted immediately before "hint".
   //!
   //! <b>Throws</b>: If "comp" throws.
   //!
   //! <b>Notes</b>: This function is used to improve performance when constructing
   //!   a node is expensive and the user does not want to have two equivalent nodes
   //!   in the tree: if there is an equivalent value
   //!   the constructed object must be discarded. Many times, the part of the
   //!   node that is used to impose the order is much cheaper to construct
   //!   than the node and this function offers the possibility to use that part
   //!   to check if the insertion will be successful.
   //!
   //!   If the check is successful, the user can construct the node and use
   //!   "insert_commit" to insert the node in constant-time. This gives a total
   //!   logarithmic complexity to the insertion: check(O(log(N)) + commit(O(1)).
   //!
   //!   "commit_data" remains valid for a subsequent "insert_unique_commit" only
   //!   if no more objects are inserted or erased from the set.
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      (const const_node_ptr & header, const node_ptr &hint, const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      //hint must be bigger than the key
      if(hint == header || comp(key, hint)){
         node_ptr prev(hint);
         //Previous value should be less than the key
         if(hint == begin_node(header) || comp((prev = base_type::prev_node(hint)), key)){
            commit_data.link_left = unique(header) || !NodeTraits::get_left(hint);
            commit_data.node      = commit_data.link_left ? hint : prev;
            if(pdepth){
               *pdepth = commit_data.node == header ? 0 : depth(commit_data.node) + 1;
            }
            return std::pair<node_ptr, bool>(node_ptr(), true);
         }
      }
      //Hint was wrong, use hintless insertion
      return insert_unique_check(header, key, comp, commit_data, pdepth);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs. "hint" is node from
   //!   the "header"'s tree.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree, using "hint" as a hint to
   //!   where it will be inserted. If "hint" is the upper_bound
   //!   the insertion takes constant time (two comparisons in the worst case).
   //!
   //! <b>Complexity</b>: Logarithmic in general, but it is amortized
   //!   constant time if new_node is inserted immediately before "hint".
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class NodePtrCompare>
   static node_ptr insert_equal
      (node_ptr h, node_ptr hint, node_ptr new_node, NodePtrCompare comp
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      insert_equal_check(h, hint, new_node, comp, commit_data, pdepth);
      insert_commit(h, new_node, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "h" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before the upper bound
   //!   according to "comp".
   //!
   //! <b>Complexity</b>: Average complexity for insert element is at
   //!   most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class NodePtrCompare>
   static node_ptr insert_equal_upper_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      insert_equal_upper_bound_check(h, new_node, comp, commit_data, pdepth);
      insert_commit(h, new_node, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "h" must be the header node of a tree.
   //!   NodePtrCompare is a function object that induces a strict weak
   //!   ordering compatible with the strict weak ordering used to create the
   //!   the tree. NodePtrCompare compares two node_ptrs.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before the lower bound
   //!   according to "comp".
   //!
   //! <b>Complexity</b>: Average complexity for insert element is at
   //!   most logarithmic.
   //!
   //! <b>Throws</b>: If "comp" throws.
   template<class NodePtrCompare>
   static node_ptr insert_equal_lower_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      insert_equal_lower_bound_check(h, new_node, comp, commit_data, pdepth);
      insert_commit(h, new_node, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "pos" must be a valid iterator or header (end) node.
   //!   "pos" must be an iterator pointing to the successor to "new_node"
   //!   once inserted according to the order of already inserted nodes. This function does not
   //!   check "pos" and this precondition must be guaranteed by the caller.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before "pos".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: If "pos" is not the successor of the newly inserted "new_node"
   //! tree invariants might be broken.
   static node_ptr insert_before
      (node_ptr header, node_ptr pos, node_ptr new_node
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      insert_before_check(header, pos, commit_data, pdepth);
      insert_commit(header, new_node, commit_data);
      return new_node;
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "new_node" must be, according to the used ordering no less than the
   //!   greatest inserted key.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before "pos".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: If "new_node" is less than the greatest inserted key
   //! tree invariants are broken. This function is slightly faster than
   //! using "insert_before".
   static void push_back
      (node_ptr header, node_ptr new_node
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      push_back_check(header, commit_data, pdepth);
      insert_commit(header, new_node, commit_data);
   }

   //! <b>Requires</b>: "header" must be the header node of a tree.
   //!   "new_node" must be, according to the used ordering, no greater than the
   //!   lowest inserted key.
   //!
   //! <b>Effects</b>: Inserts new_node into the tree before "pos".
   //!
   //! <b>Complexity</b>: Constant-time.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Note</b>: If "new_node" is greater than the lowest inserted key
   //! tree invariants are broken. This function is slightly faster than
   //! using "insert_before".
   static void push_front
      (node_ptr header, node_ptr new_node
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      insert_commit_data commit_data;
      push_front_check(header, commit_data, pdepth);
      insert_commit(header, new_node, commit_data);
   }

   //! <b>Requires</b>: 'node' can't be a header node.
   //!
   //! <b>Effects</b>: Calculates the depth of a node: the depth of a
   //! node is the length (number of edges) of the path from the root
   //! to that node. (The root node is at depth 0.)
   //!
   //! <b>Complexity</b>: Logarithmic to the number of nodes in the tree.
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t depth(const_node_ptr node)
   {
      std::size_t depth = 0;
      node_ptr p_parent;
      while(node != NodeTraits::get_parent(p_parent = NodeTraits::get_parent(node))){
         ++depth;
         node = p_parent;
      }
      return depth;
   }

   //! <b>Requires</b>: "cloner" must be a function
   //!   object taking a node_ptr and returning a new cloned node of it. "disposer" must
   //!   take a node_ptr and shouldn't throw.
   //!
   //! <b>Effects</b>: First empties target tree calling
   //!   <tt>void disposer::operator()(const node_ptr &)</tt> for every node of the tree
   //!    except the header.
   //!
   //!   Then, duplicates the entire tree pointed by "source_header" cloning each
   //!   source node with <tt>node_ptr Cloner::operator()(const node_ptr &)</tt> to obtain
   //!   the nodes of the target tree. If "cloner" throws, the cloned target nodes
   //!   are disposed using <tt>void disposer(const node_ptr &)</tt>.
   //!
   //! <b>Complexity</b>: Linear to the number of element of the source tree plus the
   //!   number of elements of tree target tree when calling this function.
   //!
   //! <b>Throws</b>: If cloner functor throws. If this happens target nodes are disposed.
   template <class Cloner, class Disposer>
   static void clone
      (const const_node_ptr & source_header, node_ptr target_header, Cloner cloner, Disposer disposer)
   {
      if(!unique(target_header)){
         clear_and_dispose(target_header, disposer);
      }

      node_ptr leftmost, rightmost;
      node_ptr new_root = clone_subtree
         (source_header, target_header, cloner, disposer, leftmost, rightmost);

      //Now update header node
      NodeTraits::set_parent(target_header, new_root);
      NodeTraits::set_left  (target_header, leftmost);
      NodeTraits::set_right (target_header, rightmost);
   }

   //! <b>Requires</b>: header must be the header of a tree, z a node
   //!    of that tree and z != header.
   //!
   //! <b>Effects</b>: Erases node "z" from the tree with header "header".
   //!
   //! <b>Complexity</b>: Amortized constant time.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static void erase(node_ptr header, node_ptr z)
   {
      data_for_rebalance ignored;
      erase(header, z, ignored);
   }

   //! <b>Requires</b>: header1 and header2 must be the headers of trees tree1 and tree2
   //!   respectively, z a non-header node of tree1. NodePtrCompare is the comparison
   //!   function of tree1..
   //!
   //! <b>Effects</b>: Transfers node "z" from tree1 to tree2 if tree1 does not contain
   //!   a node that is equivalent to z.
   //!
   //! <b>Returns</b>: True if the node was trasferred, false otherwise.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If the comparison throws.
   template<class NodePtrCompare>
   BOOST_INTRUSIVE_FORCEINLINE static bool transfer_unique
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z)
   {
      data_for_rebalance ignored;
      return transfer_unique(header1, comp, header2, z, ignored);
   }

   //! <b>Requires</b>: header1 and header2 must be the headers of trees tree1 and tree2
   //!   respectively, z a non-header node of tree1. NodePtrCompare is the comparison
   //!   function of tree1..
   //!
   //! <b>Effects</b>: Transfers node "z" from tree1 to tree2.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: If the comparison throws.
   template<class NodePtrCompare>
   BOOST_INTRUSIVE_FORCEINLINE static void transfer_equal
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z)
   {
      data_for_rebalance ignored;
      transfer_equal(header1, comp, header2, z, ignored);
   }

   //! <b>Requires</b>: node is a tree node but not the header.
   //!
   //! <b>Effects</b>: Unlinks the node and rebalances the tree.
   //!
   //! <b>Complexity</b>: Average complexity is constant time.
   //!
   //! <b>Throws</b>: Nothing.
   static void unlink(node_ptr node)
   {
      node_ptr x = NodeTraits::get_parent(node);
      if(x){
         while(!base_type::is_header(x))
            x = NodeTraits::get_parent(x);
         erase(x, node);
      }
   }

   //! <b>Requires</b>: header must be the header of a tree.
   //!
   //! <b>Effects</b>: Rebalances the tree.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear.
   static void rebalance(node_ptr header)
   {
      node_ptr root = NodeTraits::get_parent(header);
      if(root){
         rebalance_subtree(root);
      }
   }

   //! <b>Requires</b>: old_root is a node of a tree. It shall not be null.
   //!
   //! <b>Effects</b>: Rebalances the subtree rooted at old_root.
   //!
   //! <b>Returns</b>: The new root of the subtree.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear.
   static node_ptr rebalance_subtree(node_ptr old_root)
   {
      //Taken from:
      //"Tree rebalancing in optimal time and space"
      //Quentin F. Stout and Bette L. Warren

      //To avoid irregularities in the algorithm (old_root can be a
      //left or right child or even the root of the tree) just put the
      //root as the right child of its parent. Before doing this backup
      //information to restore the original relationship after
      //the algorithm is applied.
      node_ptr super_root = NodeTraits::get_parent(old_root);
      BOOST_INTRUSIVE_INVARIANT_ASSERT(super_root);

      //Get root info
      node_ptr super_root_right_backup = NodeTraits::get_right(super_root);
      bool super_root_is_header = NodeTraits::get_parent(super_root) == old_root;
      bool old_root_is_right  = is_right_child(old_root);
      NodeTraits::set_right(super_root, old_root);

      std::size_t size;
      subtree_to_vine(super_root, size);
      vine_to_subtree(super_root, size);
      node_ptr new_root = NodeTraits::get_right(super_root);

      //Recover root
      if(super_root_is_header){
         NodeTraits::set_right(super_root, super_root_right_backup);
         NodeTraits::set_parent(super_root, new_root);
      }
      else if(old_root_is_right){
         NodeTraits::set_right(super_root, new_root);
      }
      else{
         NodeTraits::set_right(super_root, super_root_right_backup);
         NodeTraits::set_left(super_root, new_root);
      }
      return new_root;
   }

   //! <b>Effects</b>: Asserts the integrity of the container with additional checks provided by the user.
   //!
   //! <b>Requires</b>: header must be the header of a tree.
   //!
   //! <b>Complexity</b>: Linear time.
   //!
   //! <b>Note</b>: The method might not have effect when asserts are turned off (e.g., with NDEBUG).
   //!   Experimental function, interface might change in future versions.
   template<class Checker>
   static void check(const const_node_ptr& header, Checker checker, typename Checker::return_type& checker_return)
   {
      const_node_ptr root_node_ptr = NodeTraits::get_parent(header);
      if (!root_node_ptr){
         // check left&right header pointers
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_left(header) == header);
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_right(header) == header);
      }
      else{
         // check parent pointer of root node
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_parent(root_node_ptr) == header);
         // check subtree from root
         check_subtree(root_node_ptr, checker, checker_return);
         // check left&right header pointers
         const_node_ptr p = root_node_ptr;
         while (NodeTraits::get_left(p)) { p = NodeTraits::get_left(p); }
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_left(header) == p);
         p = root_node_ptr;
         while (NodeTraits::get_right(p)) { p = NodeTraits::get_right(p); }
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_right(header) == p);
      }
   }

   protected:

   template<class NodePtrCompare>
   static bool transfer_unique
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z, data_for_rebalance &info)
   {
      insert_commit_data commit_data;
      bool const transferable = insert_unique_check(header1, z, comp, commit_data).second;
      if(transferable){
         erase(header2, z, info);
         insert_commit(header1, z, commit_data);
      }
      return transferable;
   }

   template<class NodePtrCompare>
   static void transfer_equal
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z, data_for_rebalance &info)
   {
      insert_commit_data commit_data;
      insert_equal_upper_bound_check(header1, z, comp, commit_data);
      erase(header2, z, info);
      insert_commit(header1, z, commit_data);
   }

   static void erase(node_ptr header, node_ptr z, data_for_rebalance &info)
   {
      node_ptr y(z);
      node_ptr x;
      const node_ptr z_left(NodeTraits::get_left(z));
      const node_ptr z_right(NodeTraits::get_right(z));

      if(!z_left){
         x = z_right;    // x might be null.
      }
      else if(!z_right){ // z has exactly one non-null child. y == z.
         x = z_left;       // x is not null.
         BOOST_ASSERT(x);
      }
      else{ //make y != z
         // y = find z's successor
         y = base_type::minimum(z_right);
         x = NodeTraits::get_right(y);     // x might be null.
      }

      node_ptr x_parent;
      const node_ptr z_parent(NodeTraits::get_parent(z));
      const bool z_is_leftchild(NodeTraits::get_left(z_parent) == z);

      if(y != z){ //has two children and y is the minimum of z
         //y is z's successor and it has a null left child.
         //x is the right child of y (it can be null)
         //Relink y in place of z and link x with y's old parent
         NodeTraits::set_parent(z_left, y);
         NodeTraits::set_left(y, z_left);
         if(y != z_right){
            //Link y with the right tree of z
            NodeTraits::set_right(y, z_right);
            NodeTraits::set_parent(z_right, y);
            //Link x with y's old parent (y must be a left child)
            x_parent = NodeTraits::get_parent(y);
            BOOST_ASSERT(NodeTraits::get_left(x_parent) == y);
            if(x)
               NodeTraits::set_parent(x, x_parent);
            //Since y was the successor and not the right child of z, it must be a left child
            NodeTraits::set_left(x_parent, x);
         }
         else{ //y was the right child of y so no need to fix x's position
            x_parent = y;
         }
         NodeTraits::set_parent(y, z_parent);
         this_type::set_child(header, y, z_parent, z_is_leftchild);
      }
      else {  // z has zero or one child, x is one child (it can be null)
         //Just link x to z's parent
         x_parent = z_parent;
         if(x)
            NodeTraits::set_parent(x, z_parent);
         this_type::set_child(header, x, z_parent, z_is_leftchild);

         //Now update leftmost/rightmost in case z was one of them
         if(NodeTraits::get_left(header) == z){
            //z_left must be null because z is the leftmost
            BOOST_ASSERT(!z_left);
            NodeTraits::set_left(header, !z_right ?
               z_parent :  // makes leftmost == header if z == root
               base_type::minimum(z_right));
         }
         if(NodeTraits::get_right(header) == z){
            //z_right must be null because z is the rightmost
            BOOST_ASSERT(!z_right);
            NodeTraits::set_right(header, !z_left ?
               z_parent :  // makes rightmost == header if z == root
               base_type::maximum(z_left));
         }
      }

      //If z had 0/1 child, y == z and one of its children (and maybe null)
      //If z had 2 children, y is the successor of z and x is the right child of y
      info.x = x;
      info.y = y;
      //If z had 0/1 child, x_parent is the new parent of the old right child of y (z's successor)
      //If z had 2 children, x_parent is the new parent of y (z_parent)
      BOOST_ASSERT(!x || NodeTraits::get_parent(x) == x_parent);
      info.x_parent = x_parent;
   }

   //! <b>Requires</b>: node is a node of the tree but it's not the header.
   //!
   //! <b>Effects</b>: Returns the number of nodes of the subtree.
   //!
   //! <b>Complexity</b>: Linear time.
   //!
   //! <b>Throws</b>: Nothing.
   static std::size_t subtree_size(const const_node_ptr & subtree)
   {
      std::size_t count = 0;
      if (subtree){
         node_ptr n = detail::uncast(subtree);
         node_ptr m = NodeTraits::get_left(n);
         while(m){
            n = m;
            m = NodeTraits::get_left(n);
         }

         while(1){
            ++count;
            node_ptr n_right(NodeTraits::get_right(n));
            if(n_right){
               n = n_right;
               m = NodeTraits::get_left(n);
               while(m){
                  n = m;
                  m = NodeTraits::get_left(n);
               }
            }
            else {
               do{
                  if (n == subtree){
                     return count;
                  }
                  m = n;
                  n = NodeTraits::get_parent(n);
               }while(NodeTraits::get_left(n) != m);
            }
         }
      }
      return count;
   }

   //! <b>Requires</b>: p is a node of a tree.
   //!
   //! <b>Effects</b>: Returns true if p is a left child.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool is_left_child(const node_ptr & p)
   {  return NodeTraits::get_left(NodeTraits::get_parent(p)) == p;  }

   //! <b>Requires</b>: p is a node of a tree.
   //!
   //! <b>Effects</b>: Returns true if p is a right child.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool is_right_child(const node_ptr & p)
   {  return NodeTraits::get_right(NodeTraits::get_parent(p)) == p;  }

   static void insert_before_check
      (node_ptr header, node_ptr pos
      , insert_commit_data &commit_data
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      node_ptr prev(pos);
      if(pos != NodeTraits::get_left(header))
         prev = base_type::prev_node(pos);
      bool link_left = unique(header) || !NodeTraits::get_left(pos);
      commit_data.link_left = link_left;
      commit_data.node = link_left ? pos : prev;
      if(pdepth){
         *pdepth = commit_data.node == header ? 0 : depth(commit_data.node) + 1;
      }
   }

   static void push_back_check
      (node_ptr header, insert_commit_data &commit_data
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      node_ptr prev(NodeTraits::get_right(header));
      if(pdepth){
         *pdepth = prev == header ? 0 : depth(prev) + 1;
      }
      commit_data.link_left = false;
      commit_data.node = prev;
   }

   static void push_front_check
      (node_ptr header, insert_commit_data &commit_data
         #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
         , std::size_t *pdepth = 0
         #endif
      )
   {
      node_ptr pos(NodeTraits::get_left(header));
      if(pdepth){
         *pdepth = pos == header ? 0 : depth(pos) + 1;
      }
      commit_data.link_left = true;
      commit_data.node = pos;
   }

   template<class NodePtrCompare>
   static void insert_equal_check
      (node_ptr header, node_ptr hint, node_ptr new_node, NodePtrCompare comp
      , insert_commit_data &commit_data
      /// @cond
      , std::size_t *pdepth = 0
      /// @endcond
      )
   {
      if(hint == header || !comp(hint, new_node)){
         node_ptr prev(hint);
         if(hint == NodeTraits::get_left(header) ||
            !comp(new_node, (prev = base_type::prev_node(hint)))){
            bool link_left = unique(header) || !NodeTraits::get_left(hint);
            commit_data.link_left = link_left;
            commit_data.node = link_left ? hint : prev;
            if(pdepth){
               *pdepth = commit_data.node == header ? 0 : depth(commit_data.node) + 1;
            }
         }
         else{
            insert_equal_upper_bound_check(header, new_node, comp, commit_data, pdepth);
         }
      }
      else{
         insert_equal_lower_bound_check(header, new_node, comp, commit_data, pdepth);
      }
   }

   template<class NodePtrCompare>
   static void insert_equal_upper_bound_check
      (node_ptr h, node_ptr new_node, NodePtrCompare comp, insert_commit_data & commit_data, std::size_t *pdepth = 0)
   {
      std::size_t depth = 0;
      node_ptr y(h);
      node_ptr x(NodeTraits::get_parent(y));

      while(x){
         ++depth;
         y = x;
         x = comp(new_node, x) ?
               NodeTraits::get_left(x) : NodeTraits::get_right(x);
      }
      if(pdepth)  *pdepth = depth;
      commit_data.link_left = (y == h) || comp(new_node, y);
      commit_data.node = y;
   }

   template<class NodePtrCompare>
   static void insert_equal_lower_bound_check
      (node_ptr h, node_ptr new_node, NodePtrCompare comp, insert_commit_data & commit_data, std::size_t *pdepth = 0)
   {
      std::size_t depth = 0;
      node_ptr y(h);
      node_ptr x(NodeTraits::get_parent(y));

      while(x){
         ++depth;
         y = x;
         x = !comp(x, new_node) ?
               NodeTraits::get_left(x) : NodeTraits::get_right(x);
      }
      if(pdepth)  *pdepth = depth;
      commit_data.link_left = (y == h) || !comp(y, new_node);
      commit_data.node = y;
   }

   static void insert_commit
      (node_ptr header, node_ptr new_node, const insert_commit_data &commit_data)
   {
      //Check if commit_data has not been initialized by a insert_unique_check call.
      BOOST_INTRUSIVE_INVARIANT_ASSERT(commit_data.node != node_ptr());
      node_ptr parent_node(commit_data.node);
      if(parent_node == header){
         NodeTraits::set_parent(header, new_node);
         NodeTraits::set_right(header, new_node);
         NodeTraits::set_left(header, new_node);
      }
      else if(commit_data.link_left){
         NodeTraits::set_left(parent_node, new_node);
         if(parent_node == NodeTraits::get_left(header))
             NodeTraits::set_left(header, new_node);
      }
      else{
         NodeTraits::set_right(parent_node, new_node);
         if(parent_node == NodeTraits::get_right(header))
             NodeTraits::set_right(header, new_node);
      }
      NodeTraits::set_parent(new_node, parent_node);
      NodeTraits::set_right(new_node, node_ptr());
      NodeTraits::set_left(new_node, node_ptr());
   }

   //Fix header and own's parent data when replacing x with own, providing own's old data with parent
   static void set_child(node_ptr header, node_ptr new_child, node_ptr new_parent, const bool link_left)
   {
      if(new_parent == header)
         NodeTraits::set_parent(header, new_child);
      else if(link_left)
         NodeTraits::set_left(new_parent, new_child);
      else
         NodeTraits::set_right(new_parent, new_child);
   }

   // rotate p to left (no header and p's parent fixup)
   static void rotate_left_no_parent_fix(node_ptr p, node_ptr p_right)
   {
      node_ptr p_right_left(NodeTraits::get_left(p_right));
      NodeTraits::set_right(p, p_right_left);
      if(p_right_left){
         NodeTraits::set_parent(p_right_left, p);
      }
      NodeTraits::set_left(p_right, p);
      NodeTraits::set_parent(p, p_right);
   }

   // rotate p to left (with header and p's parent fixup)
   static void rotate_left(node_ptr p, node_ptr p_right, node_ptr p_parent, node_ptr header)
   {
      const bool p_was_left(NodeTraits::get_left(p_parent) == p);
      rotate_left_no_parent_fix(p, p_right);
      NodeTraits::set_parent(p_right, p_parent);
      set_child(header, p_right, p_parent, p_was_left);
   }

   // rotate p to right (no header and p's parent fixup)
   static void rotate_right_no_parent_fix(node_ptr p, node_ptr p_left)
   {
      node_ptr p_left_right(NodeTraits::get_right(p_left));
      NodeTraits::set_left(p, p_left_right);
      if(p_left_right){
         NodeTraits::set_parent(p_left_right, p);
      }
      NodeTraits::set_right(p_left, p);
      NodeTraits::set_parent(p, p_left);
   }

   // rotate p to right (with header and p's parent fixup)
   static void rotate_right(node_ptr p, node_ptr p_left, node_ptr p_parent, node_ptr header)
   {
      const bool p_was_left(NodeTraits::get_left(p_parent) == p);
      rotate_right_no_parent_fix(p, p_left);
      NodeTraits::set_parent(p_left, p_parent);
      set_child(header, p_left, p_parent, p_was_left);
   }

   private:

   static void subtree_to_vine(node_ptr vine_tail, std::size_t &size)
   {
      //Inspired by LibAVL:
      //It uses a clever optimization for trees with parent pointers.
      //No parent pointer is updated when transforming a tree to a vine as
      //most of them will be overriten during compression rotations.
      //A final pass must be made after the rebalancing to updated those
      //pointers not updated by tree_to_vine + compression calls
      std::size_t len = 0;
      node_ptr remainder = NodeTraits::get_right(vine_tail);
      while(remainder){
         node_ptr tempptr = NodeTraits::get_left(remainder);
         if(!tempptr){   //move vine-tail down one
            vine_tail = remainder;
            remainder = NodeTraits::get_right(remainder);
            ++len;
         }
         else{ //rotate
            NodeTraits::set_left(remainder, NodeTraits::get_right(tempptr));
            NodeTraits::set_right(tempptr, remainder);
            remainder = tempptr;
            NodeTraits::set_right(vine_tail, tempptr);
         }
      }
      size = len;
   }

   static void compress_subtree(node_ptr scanner, std::size_t count)
   {
      while(count--){   //compress "count" spine nodes in the tree with pseudo-root scanner
         node_ptr child = NodeTraits::get_right(scanner);
         node_ptr child_right = NodeTraits::get_right(child);
         NodeTraits::set_right(scanner, child_right);
         //Avoid setting the parent of child_right
         scanner = child_right;
         node_ptr scanner_left = NodeTraits::get_left(scanner);
         NodeTraits::set_right(child, scanner_left);
         if(scanner_left)
            NodeTraits::set_parent(scanner_left, child);
         NodeTraits::set_left(scanner, child);
         NodeTraits::set_parent(child, scanner);
      }
   }

   static void vine_to_subtree(node_ptr super_root, std::size_t count)
   {
      const std::size_t one_szt = 1u;
      std::size_t leaf_nodes = count + one_szt - std::size_t(one_szt << detail::floor_log2(count + one_szt));
      compress_subtree(super_root, leaf_nodes);  //create deepest leaves
      std::size_t vine_nodes = count - leaf_nodes;
      while(vine_nodes > 1){
         vine_nodes /= 2;
         compress_subtree(super_root, vine_nodes);
      }

      //Update parents of nodes still in the in the original vine line
      //as those have not been updated by subtree_to_vine or compress_subtree
      for ( node_ptr q = super_root, p = NodeTraits::get_right(super_root)
          ; p
          ; q = p, p = NodeTraits::get_right(p)){
         NodeTraits::set_parent(p, q);
      }
   }

   //! <b>Requires</b>: "n" must be a node inserted in a tree.
   //!
   //! <b>Effects</b>: Returns a pointer to the header node of the tree.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr get_root(const node_ptr & node)
   {
      BOOST_INTRUSIVE_INVARIANT_ASSERT((!inited(node)));
      node_ptr x = NodeTraits::get_parent(node);
      if(x){
         while(!base_type::is_header(x)){
            x = NodeTraits::get_parent(x);
         }
         return x;
      }
      else{
         return node;
      }
   }

   template <class Cloner, class Disposer>
   static node_ptr clone_subtree
      (const const_node_ptr &source_parent, node_ptr target_parent
      , Cloner cloner, Disposer disposer
      , node_ptr &leftmost_out, node_ptr &rightmost_out
      )
   {
      node_ptr target_sub_root = target_parent;
      node_ptr source_root = NodeTraits::get_parent(source_parent);
      if(!source_root){
         leftmost_out = rightmost_out = source_root;
      }
      else{
         //We'll calculate leftmost and rightmost nodes while iterating
         node_ptr current = source_root;
         node_ptr insertion_point = target_sub_root = cloner(current);

         //We'll calculate leftmost and rightmost nodes while iterating
         node_ptr leftmost  = target_sub_root;
         node_ptr rightmost = target_sub_root;

         //First set the subroot
         NodeTraits::set_left(target_sub_root, node_ptr());
         NodeTraits::set_right(target_sub_root, node_ptr());
         NodeTraits::set_parent(target_sub_root, target_parent);

         dispose_subtree_disposer<Disposer> rollback(disposer, target_sub_root);
         while(true) {
            //First clone left nodes
            if( NodeTraits::get_left(current) &&
               !NodeTraits::get_left(insertion_point)) {
               current = NodeTraits::get_left(current);
               node_ptr temp = insertion_point;
               //Clone and mark as leaf
               insertion_point = cloner(current);
               NodeTraits::set_left  (insertion_point, node_ptr());
               NodeTraits::set_right (insertion_point, node_ptr());
               //Insert left
               NodeTraits::set_parent(insertion_point, temp);
               NodeTraits::set_left  (temp, insertion_point);
               //Update leftmost
               if(rightmost == target_sub_root)
                  leftmost = insertion_point;
            }
            //Then clone right nodes
            else if( NodeTraits::get_right(current) &&
                     !NodeTraits::get_right(insertion_point)){
               current = NodeTraits::get_right(current);
               node_ptr temp = insertion_point;
               //Clone and mark as leaf
               insertion_point = cloner(current);
               NodeTraits::set_left  (insertion_point, node_ptr());
               NodeTraits::set_right (insertion_point, node_ptr());
               //Insert right
               NodeTraits::set_parent(insertion_point, temp);
               NodeTraits::set_right (temp, insertion_point);
               //Update rightmost
               rightmost = insertion_point;
            }
            //If not, go up
            else if(current == source_root){
               break;
            }
            else{
               //Branch completed, go up searching more nodes to clone
               current = NodeTraits::get_parent(current);
               insertion_point = NodeTraits::get_parent(insertion_point);
            }
         }
         rollback.release();
         leftmost_out   = leftmost;
         rightmost_out  = rightmost;
      }
      return target_sub_root;
   }

   template<class Disposer>
   static void dispose_subtree(node_ptr x, Disposer disposer)
   {
      while (x){
         node_ptr save(NodeTraits::get_left(x));
         if (save) {
            // Right rotation
            NodeTraits::set_left(x, NodeTraits::get_right(save));
            NodeTraits::set_right(save, x);
         }
         else {
            save = NodeTraits::get_right(x);
            init(x);
            disposer(x);
         }
         x = save;
      }
   }

   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr lower_bound_loop
      (node_ptr x, node_ptr y, const KeyType &key, KeyNodePtrCompare comp)
   {
      while(x){
         if(comp(x, key)){
            x = NodeTraits::get_right(x);
         }
         else{
            y = x;
            x = NodeTraits::get_left(x);
         }
      }
      return y;
   }

   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr upper_bound_loop
      (node_ptr x, node_ptr y, const KeyType &key, KeyNodePtrCompare comp)
   {
      while(x){
         if(comp(key, x)){
            y = x;
            x = NodeTraits::get_left(x);
         }
         else{
            x = NodeTraits::get_right(x);
         }
      }
      return y;
   }

   template<class Checker>
   static void check_subtree(const const_node_ptr& node, Checker checker, typename Checker::return_type& check_return)
   {
      const_node_ptr left = NodeTraits::get_left(node);
      const_node_ptr right = NodeTraits::get_right(node);
      typename Checker::return_type check_return_left;
      typename Checker::return_type check_return_right;
      if (left)
      {
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_parent(left) == node);
         check_subtree(left, checker, check_return_left);
      }
      if (right)
      {
         BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_parent(right) == node);
         check_subtree(right, checker, check_return_right);
      }
      checker(node, check_return_left, check_return_right, check_return);
   }
};

/// @cond

template<class NodeTraits>
struct get_algo<BsTreeAlgorithms, NodeTraits>
{
   typedef bstree_algorithms<NodeTraits> type;
};

template <class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct get_node_checker<BsTreeAlgorithms, ValueTraits, NodePtrCompare, ExtraChecker>
{
   typedef detail::bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> type;
};

/// @endcond

}  //namespace intrusive
}  //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_BSTREE_ALGORITHMS_HPP
