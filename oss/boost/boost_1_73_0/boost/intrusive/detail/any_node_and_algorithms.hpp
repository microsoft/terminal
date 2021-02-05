/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_ANY_NODE_HPP
#define BOOST_INTRUSIVE_ANY_NODE_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/intrusive/detail/workaround.hpp>
#include <boost/intrusive/pointer_rebind.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/algo_type.hpp>
#include <cstddef>

namespace boost {
namespace intrusive {

template<class VoidPointer>
struct any_node
{
   typedef any_node                                               node;
   typedef typename pointer_rebind<VoidPointer, node>::type       node_ptr;
   typedef typename pointer_rebind<VoidPointer, const node>::type const_node_ptr;
   node_ptr    node_ptr_1;
   node_ptr    node_ptr_2;
   node_ptr    node_ptr_3;
   std::size_t size_t_1;
};

template<class VoidPointer>
struct any_list_node_traits
{
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_next(const const_node_ptr & n)
   {  return n->node_ptr_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_next(node_ptr n, node_ptr next)
   {  n->node_ptr_1 = next;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_previous(const const_node_ptr & n)
   {  return n->node_ptr_2;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_previous(node_ptr n, node_ptr prev)
   {  n->node_ptr_2 = prev;  }
};


template<class VoidPointer>
struct any_slist_node_traits
{
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_next(const const_node_ptr & n)
   {  return n->node_ptr_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_next(node_ptr n, node_ptr next)
   {  n->node_ptr_1 = next;  }
};


template<class VoidPointer>
struct any_unordered_node_traits
   :  public any_slist_node_traits<VoidPointer>
{
   typedef any_slist_node_traits<VoidPointer>                  reduced_slist_node_traits;
   typedef typename reduced_slist_node_traits::node            node;
   typedef typename reduced_slist_node_traits::node_ptr        node_ptr;
   typedef typename reduced_slist_node_traits::const_node_ptr  const_node_ptr;

   static const bool store_hash        = true;
   static const bool optimize_multikey = true;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_next(const const_node_ptr & n)
   {  return n->node_ptr_1;   }

   BOOST_INTRUSIVE_FORCEINLINE static void set_next(node_ptr n, node_ptr next)
   {  n->node_ptr_1 = next;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_prev_in_group(const const_node_ptr & n)
   {  return n->node_ptr_2;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_prev_in_group(node_ptr n, node_ptr prev)
   {  n->node_ptr_2 = prev;  }

   BOOST_INTRUSIVE_FORCEINLINE static std::size_t get_hash(const const_node_ptr & n)
   {  return n->size_t_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_hash(const node_ptr & n, std::size_t h)
   {  n->size_t_1 = h;  }
};


template<class VoidPointer>
struct any_rbtree_node_traits
{
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;

   typedef std::size_t color;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_parent(const const_node_ptr & n)
   {  return n->node_ptr_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_parent(node_ptr n, node_ptr p)
   {  n->node_ptr_1 = p;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_left(const const_node_ptr & n)
   {  return n->node_ptr_2;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_left(node_ptr n, node_ptr l)
   {  n->node_ptr_2 = l;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_right(const const_node_ptr & n)
   {  return n->node_ptr_3;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_right(node_ptr n, node_ptr r)
   {  n->node_ptr_3 = r;  }

   BOOST_INTRUSIVE_FORCEINLINE static color get_color(const const_node_ptr & n)
   {  return n->size_t_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_color(const node_ptr & n, color c)
   {  n->size_t_1 = c;  }

   BOOST_INTRUSIVE_FORCEINLINE static color black()
   {  return 0u;  }

   BOOST_INTRUSIVE_FORCEINLINE static color red()
   {  return 1u;  }
};


template<class VoidPointer>
struct any_avltree_node_traits
{
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;

   typedef std::size_t balance;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_parent(const const_node_ptr & n)
   {  return n->node_ptr_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_parent(node_ptr n, node_ptr p)
   {  n->node_ptr_1 = p;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_left(const const_node_ptr & n)
   {  return n->node_ptr_2;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_left(node_ptr n, node_ptr l)
   {  n->node_ptr_2 = l;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_right(const const_node_ptr & n)
   {  return n->node_ptr_3;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_right(node_ptr n, node_ptr r)
   {  n->node_ptr_3 = r;  }

   BOOST_INTRUSIVE_FORCEINLINE static balance get_balance(const const_node_ptr & n)
   {  return n->size_t_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_balance(const node_ptr & n, balance b)
   {  n->size_t_1 = b;  }

   BOOST_INTRUSIVE_FORCEINLINE static balance negative()
   {  return 0u;  }

   BOOST_INTRUSIVE_FORCEINLINE static balance zero()
   {  return 1u;  }

   BOOST_INTRUSIVE_FORCEINLINE static balance positive()
   {  return 2u;  }
};


template<class VoidPointer>
struct any_tree_node_traits
{
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_parent(const const_node_ptr & n)
   {  return n->node_ptr_1;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_parent(node_ptr n, node_ptr p)
   {  n->node_ptr_1 = p;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_left(const const_node_ptr & n)
   {  return n->node_ptr_2;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_left(node_ptr n, node_ptr l)
   {  n->node_ptr_2 = l;  }

   BOOST_INTRUSIVE_FORCEINLINE static node_ptr get_right(const const_node_ptr & n)
   {  return n->node_ptr_3;  }

   BOOST_INTRUSIVE_FORCEINLINE static void set_right(node_ptr n, node_ptr r)
   {  n->node_ptr_3 = r;  }
};

template<class VoidPointer>
class any_node_traits
{
   public:
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;
};

template<class VoidPointer>
class any_algorithms
{
   template <class T>
   static void function_not_available_for_any_hooks(typename detail::enable_if<detail::is_same<T, bool> >::type)
   {}

   public:
   typedef any_node<VoidPointer>          node;
   typedef typename node::node_ptr        node_ptr;
   typedef typename node::const_node_ptr  const_node_ptr;
   typedef any_node_traits<VoidPointer>   node_traits;

   //! <b>Requires</b>: node must not be part of any tree.
   //!
   //! <b>Effects</b>: After the function unique(node) == true.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Nodes</b>: If node is inserted in a tree, this function corrupts the tree.
   BOOST_INTRUSIVE_FORCEINLINE static void init(const node_ptr & node)
   {  node->node_ptr_1 = node_ptr();   };

   //! <b>Effects</b>: Returns true if node is in the same state as if called init(node)
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Throws</b>: Nothing.
   BOOST_INTRUSIVE_FORCEINLINE static bool inited(const const_node_ptr & node)
   {  return !node->node_ptr_1;  };

   BOOST_INTRUSIVE_FORCEINLINE static bool unique(const const_node_ptr & node)
   {  return !node->node_ptr_1; }

   static void unlink(const node_ptr &)
   {
      //Auto-unlink hooks and unlink() are not available for any hooks
      any_algorithms<VoidPointer>::template function_not_available_for_any_hooks<node_ptr>();
   }

   static void swap_nodes(const node_ptr &, const node_ptr &)
   {
      //Any nodes have no swap_nodes capability because they don't know
      //what algorithm they must use to unlink the node from the container
      any_algorithms<VoidPointer>::template function_not_available_for_any_hooks<node_ptr>();
   }
};

///@cond

template<class NodeTraits>
struct get_algo<AnyAlgorithm, NodeTraits>
{
   typedef typename pointer_rebind<typename NodeTraits::node_ptr, void>::type void_pointer;
   typedef any_algorithms<void_pointer> type;
};

///@endcond

} //namespace intrusive
} //namespace boost

#endif //BOOST_INTRUSIVE_ANY_NODE_HPP
