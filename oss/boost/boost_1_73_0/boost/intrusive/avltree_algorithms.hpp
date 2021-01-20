/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Daniel K. O. 2005.
// (C) Copyright Ion Gaztanaga 2007-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_AVLTREE_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_AVLTREE_ALGORITHMS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>

#include <cstddef>

#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/intrusive/detail/ebo_functor_holder.hpp>
#include <boost/intrusive/bstree_algorithms.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif


namespace boost {
namespace intrusive {

/// @cond

template<class NodeTraits, class F>
struct avltree_node_cloner
   //Use public inheritance to avoid MSVC bugs with closures
   :  public detail::ebo_functor_holder<F>
{
   typedef typename NodeTraits::node_ptr  node_ptr;
   typedef detail::ebo_functor_holder<F>  base_t;

   BOOST_INTRUSIVE_FORCEINLINE avltree_node_cloner(F f)
      :  base_t(f)
   {}

   BOOST_INTRUSIVE_FORCEINLINE node_ptr operator()(const node_ptr & p)
   {
      node_ptr n = base_t::get()(p);
      NodeTraits::set_balance(n, NodeTraits::get_balance(p));
      return n;
   }

   BOOST_INTRUSIVE_FORCEINLINE node_ptr operator()(const node_ptr & p) const
   {
      node_ptr n = base_t::get()(p);
      NodeTraits::set_balance(n, NodeTraits::get_balance(p));
      return n;
   }
};

namespace detail {

template<class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct avltree_node_checker
      : public bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker>
{
   typedef bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> base_checker_t;
   typedef ValueTraits                             value_traits;
   typedef typename value_traits::node_traits      node_traits;
   typedef typename node_traits::const_node_ptr    const_node_ptr;

   struct return_type
         : public base_checker_t::return_type
   {
      return_type() : height(0) {}
      int height;
   };

   avltree_node_checker(const NodePtrCompare& comp, ExtraChecker extra_checker)
      : base_checker_t(comp, extra_checker)
   {}

   void operator () (const const_node_ptr& p,
                     const return_type& check_return_left, const return_type& check_return_right,
                     return_type& check_return)
   {
      const int height_diff = check_return_right.height - check_return_left.height; (void)height_diff;
      BOOST_INTRUSIVE_INVARIANT_ASSERT(
         (height_diff == -1 && node_traits::get_balance(p) == node_traits::negative()) ||
         (height_diff ==  0 && node_traits::get_balance(p) == node_traits::zero()) ||
         (height_diff ==  1 && node_traits::get_balance(p) == node_traits::positive())
      );
      check_return.height = 1 +
         (check_return_left.height > check_return_right.height ? check_return_left.height : check_return_right.height);
      base_checker_t::operator()(p, check_return_left, check_return_right, check_return);
   }
};

} // namespace detail

/// @endcond

//! avltree_algorithms is configured with a NodeTraits class, which encapsulates the
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
//! <tt>balance</tt>: The type of the balance factor
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
//!
//! <tt>static balance get_balance(const_node_ptr n);</tt>
//!
//! <tt>static void set_balance(node_ptr n, balance b);</tt>
//!
//! <tt>static balance negative();</tt>
//!
//! <tt>static balance zero();</tt>
//!
//! <tt>static balance positive();</tt>
template<class NodeTraits>
class avltree_algorithms
   #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   : public bstree_algorithms<NodeTraits>
   #endif
{
   public:
   typedef typename NodeTraits::node            node;
   typedef NodeTraits                           node_traits;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef typename NodeTraits::balance         balance;

   /// @cond
   private:
   typedef bstree_algorithms<NodeTraits>  bstree_algo;

   /// @endcond

   public:
   //! This type is the information that will be
   //! filled by insert_unique_check
   typedef typename bstree_algo::insert_commit_data insert_commit_data;

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::get_header(const const_node_ptr&)
   static node_ptr get_header(const const_node_ptr & n);

   //! @copydoc ::boost::intrusive::bstree_algorithms::begin_node
   static node_ptr begin_node(const const_node_ptr & header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::end_node
   static node_ptr end_node(const const_node_ptr & header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_tree
   static void swap_tree(node_ptr header1, node_ptr header2);

   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_nodes(node_ptr,node_ptr)
   static void swap_nodes(node_ptr node1, node_ptr node2)
   {
      if(node1 == node2)
         return;

      node_ptr header1(bstree_algo::get_header(node1)), header2(bstree_algo::get_header(node2));
      swap_nodes(node1, header1, node2, header2);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::swap_nodes(node_ptr,node_ptr,node_ptr,node_ptr)
   static void swap_nodes(node_ptr node1, node_ptr header1, node_ptr node2, node_ptr header2)
   {
      if(node1 == node2)   return;

      bstree_algo::swap_nodes(node1, header1, node2, header2);
      //Swap balance
      balance c = NodeTraits::get_balance(node1);
      NodeTraits::set_balance(node1, NodeTraits::get_balance(node2));
      NodeTraits::set_balance(node2, c);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::replace_node(node_ptr,node_ptr)
   static void replace_node(node_ptr node_to_be_replaced, node_ptr new_node)
   {
      if(node_to_be_replaced == new_node)
         return;
      replace_node(node_to_be_replaced, bstree_algo::get_header(node_to_be_replaced), new_node);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::replace_node(node_ptr,node_ptr,node_ptr)
   static void replace_node(node_ptr node_to_be_replaced, node_ptr header, node_ptr new_node)
   {
      bstree_algo::replace_node(node_to_be_replaced, header, new_node);
      NodeTraits::set_balance(new_node, NodeTraits::get_balance(node_to_be_replaced));
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::unlink(node_ptr)
   static void unlink(node_ptr node)
   {
      node_ptr x = NodeTraits::get_parent(node);
      if(x){
         while(!is_header(x))
            x = NodeTraits::get_parent(x);
         erase(x, node);
      }
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::bstree_algorithms::unlink_leftmost_without_rebalance
   static node_ptr unlink_leftmost_without_rebalance(const node_ptr & header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::unique(const const_node_ptr&)
   static bool unique(const const_node_ptr & node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::size(const const_node_ptr&)
   static std::size_t size(const const_node_ptr & header);

   //! @copydoc ::boost::intrusive::bstree_algorithms::next_node(const node_ptr&)
   static node_ptr next_node(const node_ptr & node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::prev_node(const node_ptr&)
   static node_ptr prev_node(const node_ptr & node);

   //! @copydoc ::boost::intrusive::bstree_algorithms::init(node_ptr)
   static void init(const node_ptr & node);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

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
   static void init_header(node_ptr header)
   {
      bstree_algo::init_header(header);
      NodeTraits::set_balance(header, NodeTraits::zero());
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::erase(node_ptr,node_ptr)
   static node_ptr erase(node_ptr header, node_ptr z)
   {
      typename bstree_algo::data_for_rebalance info;
      bstree_algo::erase(header, z, info);
      rebalance_after_erasure(header, z, info);
      return z;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::transfer_unique
   template<class NodePtrCompare>
   static bool transfer_unique
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z)
   {
      typename bstree_algo::data_for_rebalance info;
      bool const transferred = bstree_algo::transfer_unique(header1, comp, header2, z, info);
      if(transferred){
         rebalance_after_erasure(header2, z, info);
         rebalance_after_insertion(header1, z);
      }
      return transferred;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::transfer_equal
   template<class NodePtrCompare>
   static void transfer_equal
      (node_ptr header1, NodePtrCompare comp, node_ptr header2, node_ptr z)
   {
      typename bstree_algo::data_for_rebalance info;
      bstree_algo::transfer_equal(header1, comp, header2, z, info);
      rebalance_after_erasure(header2, z, info);
      rebalance_after_insertion(header1, z);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::clone(const const_node_ptr&,node_ptr,Cloner,Disposer)
   template <class Cloner, class Disposer>
   static void clone
      (const const_node_ptr & source_header, node_ptr target_header, Cloner cloner, Disposer disposer)
   {
      avltree_node_cloner<NodeTraits, Cloner> new_cloner(cloner);
      bstree_algo::clone(source_header, target_header, new_cloner, disposer);
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::bstree_algorithms::clear_and_dispose(const node_ptr&,Disposer)
   template<class Disposer>
   static void clear_and_dispose(const node_ptr & header, Disposer disposer);

   //! @copydoc ::boost::intrusive::bstree_algorithms::lower_bound(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr lower_bound
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::upper_bound(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr upper_bound
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::find(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static node_ptr find
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::equal_range(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> equal_range
      (const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp);

   //! @copydoc ::boost::intrusive::bstree_algorithms::bounded_range(const const_node_ptr&,const KeyType&,const KeyType&,KeyNodePtrCompare,bool,bool)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, node_ptr> bounded_range
      (const const_node_ptr & header, const KeyType &lower_key, const KeyType &upper_key, KeyNodePtrCompare comp
      , bool left_closed, bool right_closed);

   //! @copydoc ::boost::intrusive::bstree_algorithms::count(const const_node_ptr&,const KeyType&,KeyNodePtrCompare)
   template<class KeyType, class KeyNodePtrCompare>
   static std::size_t count(const const_node_ptr & header, const KeyType &key, KeyNodePtrCompare comp);

   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_equal_upper_bound(node_ptr,node_ptr,NodePtrCompare)
   template<class NodePtrCompare>
   static node_ptr insert_equal_upper_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp)
   {
      bstree_algo::insert_equal_upper_bound(h, new_node, comp);
      rebalance_after_insertion(h, new_node);
      return new_node;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_equal_lower_bound(node_ptr,node_ptr,NodePtrCompare)
   template<class NodePtrCompare>
   static node_ptr insert_equal_lower_bound
      (node_ptr h, node_ptr new_node, NodePtrCompare comp)
   {
      bstree_algo::insert_equal_lower_bound(h, new_node, comp);
      rebalance_after_insertion(h, new_node);
      return new_node;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_equal(node_ptr,node_ptr,node_ptr,NodePtrCompare)
   template<class NodePtrCompare>
   static node_ptr insert_equal
      (node_ptr header, node_ptr hint, node_ptr new_node, NodePtrCompare comp)
   {
      bstree_algo::insert_equal(header, hint, new_node, comp);
      rebalance_after_insertion(header, new_node);
      return new_node;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_before(node_ptr,node_ptr,node_ptr)
   static node_ptr insert_before
      (node_ptr header, node_ptr pos, node_ptr new_node)
   {
      bstree_algo::insert_before(header, pos, new_node);
      rebalance_after_insertion(header, new_node);
      return new_node;
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::push_back(node_ptr,node_ptr)
   static void push_back(node_ptr header, node_ptr new_node)
   {
      bstree_algo::push_back(header, new_node);
      rebalance_after_insertion(header, new_node);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::push_front(node_ptr,node_ptr)
   static void push_front(node_ptr header, node_ptr new_node)
   {
      bstree_algo::push_front(header, new_node);
      rebalance_after_insertion(header, new_node);
   }

   #ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_unique_check(const const_node_ptr&,const KeyType&,KeyNodePtrCompare,insert_commit_data&)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      (const const_node_ptr & header,  const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data);

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_unique_check(const const_node_ptr&,const node_ptr&,const KeyType&,KeyNodePtrCompare,insert_commit_data&)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      (const const_node_ptr & header, const node_ptr &hint, const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_unique_commit(node_ptr,node_ptr,const insert_commit_data &)
   static void insert_unique_commit
      (node_ptr header, node_ptr new_value, const insert_commit_data &commit_data)
   {
      bstree_algo::insert_unique_commit(header, new_value, commit_data);
      rebalance_after_insertion(header, new_value);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::is_header
   static bool is_header(const const_node_ptr & p)
   {  return NodeTraits::get_balance(p) == NodeTraits::zero() && bstree_algo::is_header(p);  }


   /// @cond
   static bool verify(const node_ptr &header)
   {
      std::size_t height;
      std::size_t count;
      return verify_recursion(NodeTraits::get_parent(header), count, height);
   }

   private:

   static bool verify_recursion(node_ptr n, std::size_t &count, std::size_t &height)
   {
      if (!n){
         count = 0;
         height = 0;
         return true;
      }
      std::size_t leftcount, rightcount;
      std::size_t leftheight, rightheight;
      if(!verify_recursion(NodeTraits::get_left (n), leftcount,  leftheight) ||
         !verify_recursion(NodeTraits::get_right(n), rightcount, rightheight) ){
         return false;
      }
      count = 1u + leftcount + rightcount;
      height = 1u + (leftheight > rightheight ? leftheight : rightheight);

      //If equal height, balance must be zero
      if(rightheight == leftheight){
         if(NodeTraits::get_balance(n) != NodeTraits::zero()){
            BOOST_ASSERT(0);
            return false;
         }
      }
      //If right is taller than left, then the difference must be at least 1 and the balance positive
      else if(rightheight > leftheight){
         if(rightheight - leftheight > 1 ){
            BOOST_ASSERT(0);
            return false;
         }
         else if(NodeTraits::get_balance(n) != NodeTraits::positive()){
            BOOST_ASSERT(0);
            return false;
         }
      }
      //If left is taller than right, then the difference must be at least 1 and the balance negative
      else{
         if(leftheight - rightheight > 1 ){
            BOOST_ASSERT(0);
            return false;
         }
         else if(NodeTraits::get_balance(n) != NodeTraits::negative()){
            BOOST_ASSERT(0);
            return false;
         }
      }
      return true;
   }

   static void rebalance_after_erasure
      ( node_ptr header, node_ptr z, const typename bstree_algo::data_for_rebalance &info)
   {
      if(info.y != z){
         NodeTraits::set_balance(info.y, NodeTraits::get_balance(z));
      }
      //Rebalance avltree
      rebalance_after_erasure_restore_invariants(header, info.x, info.x_parent);
   }

   static void rebalance_after_erasure_restore_invariants(node_ptr header, node_ptr x, node_ptr x_parent)
   {
      for ( node_ptr root = NodeTraits::get_parent(header)
          ; x != root
          ; root = NodeTraits::get_parent(header), x_parent = NodeTraits::get_parent(x)) {
         const balance x_parent_balance = NodeTraits::get_balance(x_parent);
         //Don't cache x_is_leftchild or similar because x can be null and
         //equal to both x_parent_left and x_parent_right
         const node_ptr x_parent_left (NodeTraits::get_left(x_parent));
         const node_ptr x_parent_right(NodeTraits::get_right(x_parent));

         if(x_parent_balance == NodeTraits::zero()){
            NodeTraits::set_balance( x_parent, x == x_parent_right ? NodeTraits::negative() : NodeTraits::positive() );
            break;       // the height didn't change, let's stop here
         }
         else if(x_parent_balance == NodeTraits::negative()){
            if (x == x_parent_left) {  ////x is left child or x and sibling are null
               NodeTraits::set_balance(x_parent, NodeTraits::zero()); // balanced
               x = x_parent;
            }
            else {
               // x is right child (x_parent_left is the left child)
               BOOST_INTRUSIVE_INVARIANT_ASSERT(x_parent_left);
               if (NodeTraits::get_balance(x_parent_left) == NodeTraits::positive()) {
                  // x_parent_left MUST have a right child
                  BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_right(x_parent_left));
                  x = avl_rotate_left_right(x_parent, x_parent_left, header);
               }
               else {
                  avl_rotate_right(x_parent, x_parent_left, header);
                  x = x_parent_left;
               }

               // if changed from negative to NodeTraits::positive(), no need to check above
               if (NodeTraits::get_balance(x) == NodeTraits::positive()){
                  break;
               }
            }
         }
         else if(x_parent_balance == NodeTraits::positive()){
            if (x == x_parent_right) { //x is right child or x and sibling are null
               NodeTraits::set_balance(x_parent, NodeTraits::zero()); // balanced
               x = x_parent;
            }
            else {
               // x is left child (x_parent_right is the right child)
               BOOST_INTRUSIVE_INVARIANT_ASSERT(x_parent_right);
               if (NodeTraits::get_balance(x_parent_right) == NodeTraits::negative()) {
                  // x_parent_right MUST have then a left child
                  BOOST_INTRUSIVE_INVARIANT_ASSERT(NodeTraits::get_left(x_parent_right));
                  x = avl_rotate_right_left(x_parent, x_parent_right, header);
               }
               else {
                  avl_rotate_left(x_parent, x_parent_right, header);
                  x = x_parent_right;
               }
               // if changed from NodeTraits::positive() to negative, no need to check above
               if (NodeTraits::get_balance(x) == NodeTraits::negative()){
                  break;
               }
            }
         }
         else{
            BOOST_INTRUSIVE_INVARIANT_ASSERT(false);  // never reached
         }
      }
   }

   static void rebalance_after_insertion(node_ptr header, node_ptr x)
   {
      NodeTraits::set_balance(x, NodeTraits::zero());
      // Rebalance.
      for(node_ptr root = NodeTraits::get_parent(header); x != root; root = NodeTraits::get_parent(header)){
         node_ptr const x_parent(NodeTraits::get_parent(x));
         node_ptr const x_parent_left(NodeTraits::get_left(x_parent));
         const balance x_parent_balance = NodeTraits::get_balance(x_parent);
         const bool x_is_leftchild(x == x_parent_left);
         if(x_parent_balance == NodeTraits::zero()){
            // if x is left, parent will have parent->bal_factor = negative
            // else, parent->bal_factor = NodeTraits::positive()
            NodeTraits::set_balance( x_parent, x_is_leftchild ? NodeTraits::negative() : NodeTraits::positive()  );
            x = x_parent;
         }
         else if(x_parent_balance == NodeTraits::positive()){
            // if x is a left child, parent->bal_factor = zero
            if (x_is_leftchild)
               NodeTraits::set_balance(x_parent, NodeTraits::zero());
            else{        // x is a right child, needs rebalancing
               if (NodeTraits::get_balance(x) == NodeTraits::negative())
                  avl_rotate_right_left(x_parent, x, header);
               else
                  avl_rotate_left(x_parent, x, header);
            }
            break;
         }
         else if(x_parent_balance == NodeTraits::negative()){
            // if x is a left child, needs rebalancing
            if (x_is_leftchild) {
               if (NodeTraits::get_balance(x) == NodeTraits::positive())
                  avl_rotate_left_right(x_parent, x, header);
               else
                  avl_rotate_right(x_parent, x, header);
            }
            else
               NodeTraits::set_balance(x_parent, NodeTraits::zero());
            break;
         }
         else{
            BOOST_INTRUSIVE_INVARIANT_ASSERT(false);  // never reached
         }
      }
   }

   static void left_right_balancing(node_ptr a, node_ptr b, node_ptr c)
   {
      // balancing...
      const balance c_balance = NodeTraits::get_balance(c);
      const balance zero_balance = NodeTraits::zero();
      const balance posi_balance = NodeTraits::positive();
      const balance nega_balance = NodeTraits::negative();
      NodeTraits::set_balance(c, zero_balance);
      if(c_balance == nega_balance){
         NodeTraits::set_balance(a, posi_balance);
         NodeTraits::set_balance(b, zero_balance);
      }
      else if(c_balance == zero_balance){
         NodeTraits::set_balance(a, zero_balance);
         NodeTraits::set_balance(b, zero_balance);
      }
      else if(c_balance == posi_balance){
         NodeTraits::set_balance(a, zero_balance);
         NodeTraits::set_balance(b, nega_balance);
      }
      else{
         BOOST_INTRUSIVE_INVARIANT_ASSERT(false); // never reached
      }
   }

   static node_ptr avl_rotate_left_right(const node_ptr a, const node_ptr a_oldleft, node_ptr hdr)
   {  // [note: 'a_oldleft' is 'b']
      //             |                               |         //
      //             a(-2)                           c         //
      //            / \                             / \        //
      //           /   \        ==>                /   \       //
      //      (pos)b    [g]                       b     a      //
      //          / \                            / \   / \     //
      //        [d]  c                         [d]  e f  [g]   //
      //            / \                                        //
      //           e   f                                       //
      const node_ptr c = NodeTraits::get_right(a_oldleft);
      bstree_algo::rotate_left_no_parent_fix(a_oldleft, c);
      //No need to link c with a [NodeTraits::set_parent(c, a) + NodeTraits::set_left(a, c)]
      //as c is not root and another rotation is coming
      bstree_algo::rotate_right(a, c, NodeTraits::get_parent(a), hdr);
      left_right_balancing(a, a_oldleft, c);
      return c;
   }

   static node_ptr avl_rotate_right_left(const node_ptr a, const node_ptr a_oldright, node_ptr hdr)
   {  // [note: 'a_oldright' is 'b']
      //              |                               |           //
      //              a(pos)                          c           //
      //             / \                             / \          //
      //            /   \                           /   \         //
      //          [d]   b(neg)         ==>         a     b        //
      //               / \                        / \   / \       //
      //              c  [g]                    [d] e  f  [g]     //
      //             / \                                          //
      //            e   f                                         //
      const node_ptr c (NodeTraits::get_left(a_oldright));
      bstree_algo::rotate_right_no_parent_fix(a_oldright, c);
      //No need to link c with a [NodeTraits::set_parent(c, a) + NodeTraits::set_right(a, c)]
      //as c is not root and another rotation is coming.
      bstree_algo::rotate_left(a, c, NodeTraits::get_parent(a), hdr);
      left_right_balancing(a_oldright, a, c);
      return c;
   }

   static void avl_rotate_left(node_ptr x, node_ptr x_oldright, node_ptr hdr)
   {
      bstree_algo::rotate_left(x, x_oldright, NodeTraits::get_parent(x), hdr);

      // reset the balancing factor
      if (NodeTraits::get_balance(x_oldright) == NodeTraits::positive()) {
         NodeTraits::set_balance(x, NodeTraits::zero());
         NodeTraits::set_balance(x_oldright, NodeTraits::zero());
      }
      else {        // this doesn't happen during insertions
         NodeTraits::set_balance(x, NodeTraits::positive());
         NodeTraits::set_balance(x_oldright, NodeTraits::negative());
      }
   }

   static void avl_rotate_right(node_ptr x, node_ptr x_oldleft, node_ptr hdr)
   {
      bstree_algo::rotate_right(x, x_oldleft, NodeTraits::get_parent(x), hdr);

      // reset the balancing factor
      if (NodeTraits::get_balance(x_oldleft) == NodeTraits::negative()) {
         NodeTraits::set_balance(x, NodeTraits::zero());
         NodeTraits::set_balance(x_oldleft, NodeTraits::zero());
      }
      else {        // this doesn't happen during insertions
         NodeTraits::set_balance(x, NodeTraits::negative());
         NodeTraits::set_balance(x_oldleft, NodeTraits::positive());
      }
   }

   /// @endcond
};

/// @cond

template<class NodeTraits>
struct get_algo<AvlTreeAlgorithms, NodeTraits>
{
   typedef avltree_algorithms<NodeTraits> type;
};

template <class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct get_node_checker<AvlTreeAlgorithms, ValueTraits, NodePtrCompare, ExtraChecker>
{
   typedef detail::avltree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_AVLTREE_ALGORITHMS_HPP
