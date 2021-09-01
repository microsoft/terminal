/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2014-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_BSTREE_ALGORITHMS_BASE_HPP
#define BOOST_INTRUSIVE_BSTREE_ALGORITHMS_BASE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/uncast.hpp>

namespace boost {
namespace intrusive {

template<class NodeTraits>
class bstree_algorithms_base
{
   public:
   typedef typename NodeTraits::node            node;
   typedef NodeTraits                           node_traits;
   typedef typename NodeTraits::node_ptr        node_ptr;
   typedef typename NodeTraits::const_node_ptr  const_node_ptr;

   //! <b>Requires</b>: 'node' is a node from the tree except the header.
   //!
   //! <b>Effects</b>: Returns the next node of the tree.
   //!
   //! <b>Complexity</b>: Average constant time.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr next_node(const node_ptr & node)
   {
      node_ptr const n_right(NodeTraits::get_right(node));
      if(n_right){
         return minimum(n_right);
      }
      else {
         node_ptr n(node);
         node_ptr p(NodeTraits::get_parent(n));
         while(n == NodeTraits::get_right(p)){
            n = p;
            p = NodeTraits::get_parent(p);
         }
         return NodeTraits::get_right(n) != p ? p : n;
      }
   }

   //! <b>Requires</b>: 'node' is a node from the tree except the leftmost node.
   //!
   //! <b>Effects</b>: Returns the previous node of the tree.
   //!
   //! <b>Complexity</b>: Average constant time.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr prev_node(const node_ptr & node)
   {
      if(is_header(node)){
         //return NodeTraits::get_right(node);
         return maximum(NodeTraits::get_parent(node));
      }
      else if(NodeTraits::get_left(node)){
         return maximum(NodeTraits::get_left(node));
      }
      else {
         node_ptr p(node);
         node_ptr x = NodeTraits::get_parent(p);
         while(p == NodeTraits::get_left(x)){
            p = x;
            x = NodeTraits::get_parent(x);
         }
         return x;
      }
   }

   //! <b>Requires</b>: 'node' is a node of a tree but not the header.
   //!
   //! <b>Effects</b>: Returns the minimum node of the subtree starting at p.
   //!
   //! <b>Complexity</b>: Logarithmic to the size of the subtree.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr minimum(node_ptr node)
   {
      for(node_ptr p_left = NodeTraits::get_left(node)
         ;p_left
         ;p_left = NodeTraits::get_left(node)){
         node = p_left;
      }
      return node;
   }

   //! <b>Requires</b>: 'node' is a node of a tree but not the header.
   //!
   //! <b>Effects</b>: Returns the maximum node of the subtree starting at p.
   //!
   //! <b>Complexity</b>: Logarithmic to the size of the subtree.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr maximum(node_ptr node)
   {
      for(node_ptr p_right = NodeTraits::get_right(node)
         ;p_right
         ;p_right = NodeTraits::get_right(node)){
         node = p_right;
      }
      return node;
   }

   //! <b>Requires</b>: p is a node of a tree.
   //!
   //! <b>Effects</b>: Returns true if p is the header of the tree.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   static bool is_header(const const_node_ptr & p)
   {
      node_ptr p_left (NodeTraits::get_left(p));
      node_ptr p_right(NodeTraits::get_right(p));
      if(!NodeTraits::get_parent(p) || //Header condition when empty tree
         (p_left && p_right &&         //Header always has leftmost and rightmost
            (p_left == p_right ||      //Header condition when only node
               (NodeTraits::get_parent(p_left)  != p ||
                NodeTraits::get_parent(p_right) != p ))
               //When tree size > 1 headers can't be leftmost's
               //and rightmost's parent
          )){
         return true;
      }
      return false;
   }

   //! <b>Requires</b>: 'node' is a node of the tree or a header node.
   //!
   //! <b>Effects</b>: Returns the header of the tree.
   //!
   //! <b>Complexity</b>: Logarithmic.
   //!
   //! <b>Throws</b>: Nothing.
   static node_ptr get_header(const const_node_ptr & node)
   {
      node_ptr n(detail::uncast(node));
      node_ptr p(NodeTraits::get_parent(node));
      //If p is null, then n is the header of an empty tree
      if(p){
         //Non-empty tree, check if n is neither root nor header
         node_ptr pp(NodeTraits::get_parent(p));
         //If granparent is not equal to n, then n is neither root nor header,
         //the try the fast path
         if(n != pp){
            do{
               n = p;
               p = pp;
               pp = NodeTraits::get_parent(pp);
            }while(n != pp);
            n = p;
         }
         //Check if n is root or header when size() > 0
         else if(!bstree_algorithms_base::is_header(n)){
            n = p;
         }
      }
      return n;
   }
};

}  //namespace intrusive
}  //namespace boost

#endif //BOOST_INTRUSIVE_BSTREE_ALGORITHMS_BASE_HPP
