/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
//
// The tree destruction algorithm is based on Julienne Walker and The EC Team code:
//
// This code is in the public domain. Anyone may use it or change it in any way that
// they see fit. The author assumes no responsibility for damages incurred through
// use of the original code or any variations thereof.
//
// It is requested, but not required, that due credit is given to the original author
// and anyone who has modified the code through a header comment, such as this one.

#ifndef BOOST_INTRUSIVE_RBTREE_ALGORITHMS_HPP
#define BOOST_INTRUSIVE_RBTREE_ALGORITHMS_HPP

#include <boost/intrusive/detail/config_begin.hpp>
#include <boost/intrusive/intrusive_fwd.hpp>

#include <cstddef>

#include <boost/intrusive/detail/assert.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <boost/intrusive/bstree_algorithms.hpp>
#include <boost/intrusive/detail/ebo_functor_holder.hpp>

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

namespace boost {
namespace intrusive {

#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

template<class NodeTraits, class F>
struct rbtree_node_cloner
   //Use public inheritance to avoid MSVC bugs with closures
   :  public detail::ebo_functor_holder<F>
{
   typedef typename NodeTraits::node_ptr  node_ptr;
   typedef detail::ebo_functor_holder<F>  base_t;

   explicit rbtree_node_cloner(F f)
      :  base_t(f)
   {}

   BOOST_INTRUSIVE_FORCEINLINE node_ptr operator()(node_ptr p)
   {
      node_ptr n = base_t::get()(p);
      NodeTraits::set_color(n, NodeTraits::get_color(p));
      return n;
   }
};

namespace detail {

template<class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct rbtree_node_checker
   : public bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker>
{
   typedef bstree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> base_checker_t;
   typedef ValueTraits                             value_traits;
   typedef typename value_traits::node_traits      node_traits;
   typedef typename node_traits::const_node_ptr    const_node_ptr;
   typedef typename node_traits::node_ptr          node_ptr;

   struct return_type
         : public base_checker_t::return_type
   {
      return_type() : black_count_(0) {}
      std::size_t black_count_;
   };

   rbtree_node_checker(const NodePtrCompare& comp, ExtraChecker extra_checker)
      : base_checker_t(comp, extra_checker)
   {}

   void operator () (const const_node_ptr& p,
                     const return_type& check_return_left, const return_type& check_return_right,
                     return_type& check_return)
   {

      if (node_traits::get_color(p) == node_traits::red()){
         //Red nodes have black children
         const node_ptr p_left(node_traits::get_left(p));   (void)p_left;
         const node_ptr p_right(node_traits::get_right(p)); (void)p_right;
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!p_left  || node_traits::get_color(p_left)  == node_traits::black());
         BOOST_INTRUSIVE_INVARIANT_ASSERT(!p_right || node_traits::get_color(p_right) == node_traits::black());
         //Red node can't be root
         BOOST_INTRUSIVE_INVARIANT_ASSERT(node_traits::get_parent(node_traits::get_parent(p)) != p);
      }
      //Every path to p contains the same number of black nodes
      const std::size_t l_black_count = check_return_left.black_count_;
      BOOST_INTRUSIVE_INVARIANT_ASSERT(l_black_count == check_return_right.black_count_);
      check_return.black_count_ = l_black_count +
         static_cast<std::size_t>(node_traits::get_color(p) == node_traits::black());
      base_checker_t::operator()(p, check_return_left, check_return_right, check_return);
   }
};

} // namespace detail

#endif   //#ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED

//! rbtree_algorithms provides basic algorithms to manipulate
//! nodes forming a red-black tree. The insertion and deletion algorithms are
//! based on those in Cormen, Leiserson, and Rivest, Introduction to Algorithms
//! (MIT Press, 1990), except that
//!
//! (1) the header node is maintained with links not only to the root
//! but also to the leftmost node of the tree, to enable constant time
//! begin(), and to the rightmost node of the tree, to enable linear time
//! performance when used with the generic set algorithms (set_union,
//! etc.);
//!
//! (2) when a node being deleted has two children its successor node is
//! relinked into its place, rather than copied, so that the only
//! pointers invalidated are those referring to the deleted node.
//!
//! rbtree_algorithms is configured with a NodeTraits class, which encapsulates the
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
//! <tt>color</tt>: The type that can store the color of a node
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
//! <tt>static color get_color(const_node_ptr n);</tt>
//!
//! <tt>static void set_color(node_ptr n, color c);</tt>
//!
//! <tt>static color black();</tt>
//!
//! <tt>static color red();</tt>
template<class NodeTraits>
class rbtree_algorithms
   #ifndef BOOST_INTRUSIVE_DOXYGEN_INVOKED
   : public bstree_algorithms<NodeTraits>
   #endif
{
   public:
   typedef NodeTraits                           node_traits;
   typedef typename NodeTraits::node            node;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;
   typedef typename NodeTraits::color           color;

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
      //Swap color
      color c = NodeTraits::get_color(node1);
      NodeTraits::set_color(node1, NodeTraits::get_color(node2));
      NodeTraits::set_color(node2, c);
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
      NodeTraits::set_color(new_node, NodeTraits::get_color(node_to_be_replaced));
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::unlink(node_ptr)
   static void unlink(const node_ptr& node)
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

   //! @copydoc ::boost::intrusive::bstree_algorithms::init_header(node_ptr)
   BOOST_INTRUSIVE_FORCEINLINE static void init_header(node_ptr header)
   {
      bstree_algo::init_header(header);
      NodeTraits::set_color(header, NodeTraits::red());
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
      (const_node_ptr source_header, node_ptr target_header, Cloner cloner, Disposer disposer)
   {
      rbtree_node_cloner<NodeTraits, Cloner> new_cloner(cloner);
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

   //! @copydoc ::boost::intrusive::bstree_algorithms::find(const const_node_ptr&, const KeyType&,KeyNodePtrCompare)
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
      (const_node_ptr header,  const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data);

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_unique_check(const const_node_ptr&,const node_ptr&,const KeyType&,KeyNodePtrCompare,insert_commit_data&)
   template<class KeyType, class KeyNodePtrCompare>
   static std::pair<node_ptr, bool> insert_unique_check
      (const_node_ptr header, node_ptr hint, const KeyType &key
      ,KeyNodePtrCompare comp, insert_commit_data &commit_data);
   #endif   //#ifdef BOOST_INTRUSIVE_DOXYGEN_INVOKED

   //! @copydoc ::boost::intrusive::bstree_algorithms::insert_unique_commit(node_ptr,node_ptr,const insert_commit_data&)
   static void insert_unique_commit
      (node_ptr header, node_ptr new_value, const insert_commit_data &commit_data)
   {
      bstree_algo::insert_unique_commit(header, new_value, commit_data);
      rebalance_after_insertion(header, new_value);
   }

   //! @copydoc ::boost::intrusive::bstree_algorithms::is_header
   static bool is_header(const const_node_ptr & p)
   {
      return NodeTraits::get_color(p) == NodeTraits::red() &&
            bstree_algo::is_header(p);
   }

   /// @cond
   private:

   static void rebalance_after_erasure
      ( node_ptr header, node_ptr z, const typename bstree_algo::data_for_rebalance &info)
   {
      color new_z_color;
      if(info.y != z){
         new_z_color = NodeTraits::get_color(info.y);
         NodeTraits::set_color(info.y, NodeTraits::get_color(z));
      }
      else{
         new_z_color = NodeTraits::get_color(z);
      }
      //Rebalance rbtree if needed
      if(new_z_color != NodeTraits::red()){
         rebalance_after_erasure_restore_invariants(header, info.x, info.x_parent);
      }
   }

   static void rebalance_after_erasure_restore_invariants(node_ptr header, node_ptr x, node_ptr x_parent)
   {
      while(1){
         if(x_parent == header || (x && NodeTraits::get_color(x) != NodeTraits::black())){
            break;
         }
         //Don't cache x_is_leftchild or similar because x can be null and
         //equal to both x_parent_left and x_parent_right
         const node_ptr x_parent_left(NodeTraits::get_left(x_parent));
         if(x == x_parent_left){ //x is left child
            node_ptr w = NodeTraits::get_right(x_parent);
            BOOST_INTRUSIVE_INVARIANT_ASSERT(w);
            if(NodeTraits::get_color(w) == NodeTraits::red()){
               NodeTraits::set_color(w, NodeTraits::black());
               NodeTraits::set_color(x_parent, NodeTraits::red());
               bstree_algo::rotate_left(x_parent, w, NodeTraits::get_parent(x_parent), header);
               w = NodeTraits::get_right(x_parent);
               BOOST_INTRUSIVE_INVARIANT_ASSERT(w);
            }
            node_ptr const w_left (NodeTraits::get_left(w));
            node_ptr const w_right(NodeTraits::get_right(w));
            if((!w_left  || NodeTraits::get_color(w_left)  == NodeTraits::black()) &&
               (!w_right || NodeTraits::get_color(w_right) == NodeTraits::black())){
               NodeTraits::set_color(w, NodeTraits::red());
               x = x_parent;
               x_parent = NodeTraits::get_parent(x_parent);
            }
            else {
               if(!w_right || NodeTraits::get_color(w_right) == NodeTraits::black()){
                  NodeTraits::set_color(w_left, NodeTraits::black());
                  NodeTraits::set_color(w, NodeTraits::red());
                  bstree_algo::rotate_right(w, w_left, NodeTraits::get_parent(w), header);
                  w = NodeTraits::get_right(x_parent);
                  BOOST_INTRUSIVE_INVARIANT_ASSERT(w);
               }
               NodeTraits::set_color(w, NodeTraits::get_color(x_parent));
               NodeTraits::set_color(x_parent, NodeTraits::black());
               const node_ptr new_wright(NodeTraits::get_right(w));
               if(new_wright)
                  NodeTraits::set_color(new_wright, NodeTraits::black());
               bstree_algo::rotate_left(x_parent, NodeTraits::get_right(x_parent), NodeTraits::get_parent(x_parent), header);
               break;
            }
         }
         else {
            // same as above, with right_ <-> left_.
            node_ptr w = x_parent_left;
            if(NodeTraits::get_color(w) == NodeTraits::red()){
               NodeTraits::set_color(w, NodeTraits::black());
               NodeTraits::set_color(x_parent, NodeTraits::red());
               bstree_algo::rotate_right(x_parent, w, NodeTraits::get_parent(x_parent), header);
               w = NodeTraits::get_left(x_parent);
               BOOST_INTRUSIVE_INVARIANT_ASSERT(w);
            }
            node_ptr const w_left (NodeTraits::get_left(w));
            node_ptr const w_right(NodeTraits::get_right(w));
            if((!w_right || NodeTraits::get_color(w_right) == NodeTraits::black()) &&
               (!w_left  || NodeTraits::get_color(w_left)  == NodeTraits::black())){
               NodeTraits::set_color(w, NodeTraits::red());
               x = x_parent;
               x_parent = NodeTraits::get_parent(x_parent);
            }
            else {
               if(!w_left || NodeTraits::get_color(w_left) == NodeTraits::black()){
                  NodeTraits::set_color(w_right, NodeTraits::black());
                  NodeTraits::set_color(w, NodeTraits::red());
                  bstree_algo::rotate_left(w, w_right, NodeTraits::get_parent(w), header);
                  w = NodeTraits::get_left(x_parent);
                  BOOST_INTRUSIVE_INVARIANT_ASSERT(w);
               }
               NodeTraits::set_color(w, NodeTraits::get_color(x_parent));
               NodeTraits::set_color(x_parent, NodeTraits::black());
               const node_ptr new_wleft(NodeTraits::get_left(w));
               if(new_wleft)
                  NodeTraits::set_color(new_wleft, NodeTraits::black());
               bstree_algo::rotate_right(x_parent, NodeTraits::get_left(x_parent), NodeTraits::get_parent(x_parent), header);
               break;
            }
         }
      }
      if(x)
         NodeTraits::set_color(x, NodeTraits::black());
   }

   static void rebalance_after_insertion(node_ptr header, node_ptr p)
   {
      NodeTraits::set_color(p, NodeTraits::red());
      while(1){
         node_ptr p_parent(NodeTraits::get_parent(p));
         const node_ptr p_grandparent(NodeTraits::get_parent(p_parent));
         if(p_parent == header || NodeTraits::get_color(p_parent) == NodeTraits::black() || p_grandparent == header){
            break;
         }

         NodeTraits::set_color(p_grandparent, NodeTraits::red());
         node_ptr const p_grandparent_left (NodeTraits::get_left (p_grandparent));
         bool const p_parent_is_left_child = p_parent == p_grandparent_left;
         node_ptr const x(p_parent_is_left_child ? NodeTraits::get_right(p_grandparent) : p_grandparent_left);

         if(x && NodeTraits::get_color(x) == NodeTraits::red()){
            NodeTraits::set_color(x, NodeTraits::black());
            NodeTraits::set_color(p_parent, NodeTraits::black());
            p = p_grandparent;
         }
         else{ //Final step
            const bool p_is_left_child(NodeTraits::get_left(p_parent) == p);
            if(p_parent_is_left_child){ //p_parent is left child
               if(!p_is_left_child){ //p is right child
                  bstree_algo::rotate_left_no_parent_fix(p_parent, p);
                  //No need to link p and p_grandparent:
                  //    [NodeTraits::set_parent(p, p_grandparent) + NodeTraits::set_left(p_grandparent, p)]
                  //as p_grandparent is not the header, another rotation is coming and p_parent
                  //will be the left child of p_grandparent
                  p_parent = p;
               }
               bstree_algo::rotate_right(p_grandparent, p_parent, NodeTraits::get_parent(p_grandparent), header);
            }
            else{  //p_parent is right child
               if(p_is_left_child){ //p is left child
                  bstree_algo::rotate_right_no_parent_fix(p_parent, p);
                  //No need to link p and p_grandparent:
                  //    [NodeTraits::set_parent(p, p_grandparent) + NodeTraits::set_right(p_grandparent, p)]
                  //as p_grandparent is not the header, another rotation is coming and p_parent
                  //will be the right child of p_grandparent
                  p_parent = p;
               }
               bstree_algo::rotate_left(p_grandparent, p_parent, NodeTraits::get_parent(p_grandparent), header);
            }
            NodeTraits::set_color(p_parent, NodeTraits::black());
            break;
         }
      }
      NodeTraits::set_color(NodeTraits::get_parent(header), NodeTraits::black());
   }
   /// @endcond
};

/// @cond

template<class NodeTraits>
struct get_algo<RbTreeAlgorithms, NodeTraits>
{
   typedef rbtree_algorithms<NodeTraits> type;
};

template <class ValueTraits, class NodePtrCompare, class ExtraChecker>
struct get_node_checker<RbTreeAlgorithms, ValueTraits, NodePtrCompare, ExtraChecker>
{
    typedef detail::rbtree_node_checker<ValueTraits, NodePtrCompare, ExtraChecker> type;
};

/// @endcond

} //namespace intrusive
} //namespace boost

#include <boost/intrusive/detail/config_end.hpp>

#endif //BOOST_INTRUSIVE_RBTREE_ALGORITHMS_HPP
